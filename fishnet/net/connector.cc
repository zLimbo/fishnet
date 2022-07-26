#include "fishnet/net/connector.h"

#include <cerrno>

#include "fishnet/base/logging.h"
#include "fishnet/net/channel.h"
#include "fishnet/net/event_loop.h"
#include "fishnet/net/inet_address.h"
#include "fishnet/net/sockets_ops.h"
#include "fishnet/net/tcp_server.h"

using namespace fishnet;
using namespace fishnet::net;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop),
      serverAddr_(serverAddr),
      connect_(false),
      state_(States::kDisconnected),
      retryDelaysMs_(kInitRetryDelayMs) {
    LOG_DEBUG << "ctor[" << this << "]";
}

Connector::~Connector() {
    LOG_DEBUG << "dtor[" << this << "]";
    assert(!channel_);
}

void Connector::start() {
    connect_ = true;
    loop_->runInLoop(
        std::bind(&Connector::startInLoop, this));  // FIXME: unsafe
}

void Connector::startInLoop() {
    loop_->assertInLoopThread();
    assert(state_ == States::kDisconnected);
    if (connect_) {
        connect();
    } else {
        LOG_DEBUG << "do not connect";
    }
}

void Connector::stop() {
    connect_ = false;
    loop_->queueInLoop(
        std::bind(&Connector::stopInLoop, this));  // FIXME: unsafe
    // FIXME: cancel timer
}

void Connector::stopInLoop() {
    loop_->assertInLoopThread();
    if (state_ == States::kConnecting) {
        setState(States::kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::connect() {
    int sockfd = sockets::createNonblockingOrDie(serverAddr_.family());
    int ret = sockets::connect(sockfd, serverAddr_.getSockAddr());
    int savedErrno = (ret == 0) ? 0 : errno;
    switch (savedErrno) {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            connecting(sockfd);
            break;
        case EAGAIN:
        case EADDRINUSE:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry(sockfd);
            break;
        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            LOG_SYSERR << "connect error in Connector::startInLoop "
                       << savedErrno;
            sockets::close(sockfd);
            break;

        default:
            LOG_SYSERR << "Unexpected error in Connector::startInLoop "
                       << savedErrno;
            sockets::close(sockfd);
            break;
    }
}

void Connector::restart() {
    loop_->assertInLoopThread();
    setState(States::kDisconnected);
    retryDelaysMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

void Connector::connecting(int sockfd) {
    setState(States::kConnecting);
    assert(!channel_);
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback(std::bind(&Connector::handleWrite, this));
    channel_->setErrorCallback(std::bind(&Connector::handleError, this));
    channel_->enableWriting();
}

int Connector::removeAndResetChannel() {
    channel_->disableAll();
    channel_->remove();
    int sockfd = channel_->fd();

    loop_->queueInLoop(std::bind(&Connector::resetChannel, this));
    return sockfd;
}

void Connector::resetChannel() {
    channel_.reset();
}

void Connector::handleWrite() {
    LOG_TRACE << "Connector::handleWrite " << static_cast<int>(state_);

    if (state_ == States::kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        if (err) {
            LOG_WARN << "Connector::handleWrite - SO_ERROR = " << err << " "
                     << strerror_tl(err);
            retry(sockfd);
        } else if (sockets::isSelfConnect(sockfd)) {
            LOG_WARN << "Connector::handleWrite - Self connect ";
            retry(sockfd);
        } else {
            setState(States::kConnected);
            if (connect_) {
                newConnectionCallback_(sockfd);
            } else {
                sockets::close(sockfd);
            }
        }
    } else {
        assert(state_ == States::kDisconnected);
    }
}

void Connector::handleError() {
    LOG_ERROR << "Connector::handleError state=" << static_cast<int>(state_);
    if (state_ == States::kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
        retry(sockfd);
    }
}

void Connector::retry(int sockfd) {
    sockets::close(sockfd);
    setState(States::kDisconnected);
    if (connect_) {
        LOG_INFO << "Connector::retry - Retry connecting to "
                 << serverAddr_.toIpPort() << " in " << retryDelaysMs_
                 << " milliseconds. ";
        loop_->runAfter(retryDelaysMs_ / 1000.0,
                        std::bind(&Connector::startInLoop, shared_from_this()));
        retryDelaysMs_ = std::min(retryDelaysMs_ * 2, kMaxRetryDelayMs);
    } else {
        LOG_DEBUG << "do not connect";
    }
}
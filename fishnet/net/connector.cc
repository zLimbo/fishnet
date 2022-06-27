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

void Connector::connecting(int sockfd) {}
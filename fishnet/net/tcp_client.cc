#include "fishnet/net/tcp_client.h"

#include <cstdio>

#include "fishnet/base/logging.h"
#include "fishnet/base/mutex.h"
#include "fishnet/net/callbacks.h"
#include "fishnet/net/connector.h"
#include "fishnet/net/event_loop.h"
#include "fishnet/net/inet_address.h"
#include "fishnet/net/sockets_ops.h"
#include "fishnet/net/tcp_connection.h"

using namespace fishnet;
using namespace fishnet::net;

namespace fishnet {

namespace net {

namespace detail {

void removeConnection(EventLoop* loop, const TcpConnectionPtr& conn) {
    loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}

void removeConnector(const ConnectorPtr& connector) {}

}  // namespace detail
}  // namespace net
}  // namespace fishnet

using namespace fishnet;
using namespace fishnet::net;

TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr,
                     const string& nameArg)
    : loop_(CHECK_NOTNULL(loop)),
      connector_(new Connector(loop, serverAddr)),
      name_(nameArg),
      connectionCallback_(defaultConnectionCallback),
      messageCallback_(defaultMessageCallback),
      retry_(false),
      connect_(true),
      nextConnId_(1) {
    connector_->setNewConnectionCallback(
        std::bind(&TcpClient::newConnection, this, _1));
    LOG_INFO << "TcpClient::TcpClient[" << name_ << "] - connector "
             << get_pointer(connector_);
}

TcpClient::~TcpClient() {
    LOG_INFO << "TcpClient::~TcpClient[" << name_ << "[ - connector "
             << get_pointer(connector_);
    TcpConnectionPtr conn;
    bool unique = false;
    {
        MutexLockGuard lock(mutex_);
        unique = connection_.unique();
        conn = connection_;
    }
    if (conn) {
        assert(loop_ == conn->getLoop());

        CloseCallback cb = std::bind(&detail::removeConnection, loop_, _1);
        loop_->runInLoop(std::bind(&TcpConnection::setCloseCallback, conn, cb));
        if (unique) {
            conn->forceClose();
        }
    } else {
        connector_->stop();
        loop_->runAfter(1, std::bind(&detail::removeConnector, connector_));
    }
}

void TcpClient::connect() {
    LOG_INFO << "TcpClient::connect[" << name_ << "] - connecting to "
             << connector_->serverAddress().toIpPort();
    connect_ = true;
    connector_->start();
}

void TcpClient::disconnect() {
    connect_ = false;
    {
        MutexLockGuard lock(mutex_);
        if (connection_) {
            connection_->shutdown();
        }
    }
}

void TcpClient::stop() {
    connect_ = false;
    connector_->stop();
}

void TcpClient::newConnection(int sockfd) {
    loop_->assertInLoopThread();
    InetAddress peerAddr(sockets::getPeerAddr(sockfd));
    char buf[32];
    snprintf(buf, sizeof(buf), ":%s#%d", peerAddr.toIpPort().c_str(),
             nextConnId_);
    ++nextConnId_;
    string connName = name_ + buf;

    InetAddress localAddr{sockets::getLocalAddr(sockfd)};

    TcpConnectionPtr conn{
        new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr)};
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpClient::removeConnection, this, _1));
    {
        MutexLockGuard lock{mutex_};
        connection_ = conn;
    }
    conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn) {
    loop_->assertInLoopThread();
    assert(loop_ == conn->getLoop());

    {
        MutexLockGuard lock{mutex_};
        assert(connection_ == conn);
        connection_.reset();
    }

    loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));

    if (retry_ && connect_) {
        LOG_INFO << "TcpClient::connect[" << name_ << "] - Reconnecting to "
                 << connector_->serverAddress().toIpPort();
        connector_->restart();
    }
}
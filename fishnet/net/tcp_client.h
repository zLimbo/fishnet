#pragma once

#include "fishnet/base/mutex.h"
#include "fishnet/base/noncopyable.h"
#include "fishnet/net/callbacks.h"
#include "fishnet/net/channel.h"
#include "fishnet/net/inet_address.h"
#include "fishnet/net/tcp_connection.h"

namespace fishnet {

namespace net {

class Connector;

using ConnectorPtr = std::shared_ptr<Connector>;

class TcpClient : noncopyable {
public:
    TcpClient(EventLoop* loop, const InetAddress& serverAddr,
              const string& nameArg);

    ~TcpClient();

    void connect();

    void disconnect();

    void stop();

    TcpConnectionPtr connection() const {
        MutexLockGuard lock(mutex_);
        return connection_;
    }

    EventLoop* getLoop() const {
        return loop_;
    }

    bool retry() const {
        return retry_;
    }

    void enableRetry() {
        retry_ = true;
    }

    const string& name() const {
        return name_;
    }

    // Not thread safe
    void setConnectionCallback(ConnectionCallback cb) {
        connectionCallback_ = std::move(cb);
    }

    // Not thread safe
    void setMessageCallback(MessageCallback cb) {
        messageCallback_ = std::move(cb);
    }

    // Not thread safe
    void setWriteCompleteCallback(WriteCompleteCallback cb) {
        writeCompleteCallback_ = std::move(cb);
    }

private:
    void newConnection(int sockfd);

    void removeConnection(const TcpConnectionPtr& conn);

    EventLoop* loop_;
    ConnectorPtr connector_;  // avoid revealing Connector
    const string name_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    bool retry_;    // atomic
    bool connect_;  // atomic
    // always in loop thread
    int nextConnId_;
    mutable MutexLock mutex_;
    TcpConnectionPtr connection_ GUARDED_BY(mutex_);
};

}  // namespace net
}  // namespace fishnet
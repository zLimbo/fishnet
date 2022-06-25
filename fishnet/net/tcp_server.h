#pragma once

#include <functional>
#include <map>

#include "fishnet/base/atomic.h"
#include "fishnet/base/noncopyable.h"
#include "fishnet/base/types.h"
#include "fishnet/net/acceptor.h"
#include "fishnet/net/callbacks.h"
#include "fishnet/net/event_loop_thread_pool.h"
#include "fishnet/net/inet_address.h"
#include "fishnet/net/tcp_connection.h"

namespace fishnet {

namespace net {

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

class TcpServer : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum class Option { kNoReusePort, kReusePort };

    TcpServer(EventLoop* loop, const InetAddress& listenAddr,
              const string& nameArg, Option option = Option::kNoReusePort);

    ~TcpServer();

    const string& ipPort() const {
        return ipPort_;
    }

    const string& name() const {
        return name_;
    }

    EventLoop* getLoop() const {
        return loop_;
    }

    /// Set the number of threads for handling input.
    ///
    /// Always accepts new connection in loop's thread.
    /// Must be called before @c start
    /// @param numThreads
    /// - 0 means all I/O in loop's thread, no thread will created.
    ///   this is the default value.
    /// - 1 means all I/O in another thread.
    /// - N means a thread pool with N threads, new connections
    ///   are assigned on a round-robin basis.
    void setThreadNum(int numThreads);

    void setThreadInitCallback(const ThreadInitCallback& cb) {
        threadInitCallback_ = cb;
    }

    /// valid after calling start()
    std::shared_ptr<EventLoopThreadPool> threadPool() {
        return threadPool_;
    }

    /// Starts the server if it's not listening.
    ///
    /// It's harmless to call it multiple times.
    /// Thread safe.
    void start();

    /// Set connection callback.
    /// Not thread safe.
    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }

    /// Set connection callback.
    /// Not thread safe.
    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }

    /// Set write complete callback.
    /// Not thread safe.
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
        writeCompleteCallback_ = cb;
    }

private:
    /// Not thread safe, but in loop
    void newConnection(int sockfd, const InetAddress& peerAddr);

    /// Not thread safe, but in loop
    void removeConnection(const TcpConnectionPtr& conn);

    /// Not thread safe, but in loop
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::map<string, TcpConnectionPtr>;

    EventLoop* loop_;  // the acceptor loop
    const string ipPort_;
    const string name_;
    std::unique_ptr<Acceptor> acceptor_;  // avoid revealing Acceptor
    std::shared_ptr<EventLoopThreadPool> threadPool_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;
    AtomicInt32 started_;
    // always in loop thread
    int nextConnId_;
    ConnectionMap connections_;
};

}  // namespace net
}  // namespace fishnet
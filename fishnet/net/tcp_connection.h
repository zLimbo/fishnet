#pragma once

#include <boost/any.hpp>
#include <memory>

#include "fishnet/base/noncopyable.h"
#include "fishnet/base/string_piece.h"
#include "fishnet/base/timestamp.h"
#include "fishnet/base/types.h"
#include "fishnet/net/buffer.h"
#include "fishnet/net/callbacks.h"
#include "fishnet/net/channel.h"
#include "fishnet/net/event_loop.h"
#include "fishnet/net/inet_address.h"
#include "fishnet/net/socket.h"

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace fishnet {

namespace net {

class Channel;
class EventLoop;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(EventLoop* loop, const string& name, int sockfd,
                  const InetAddress& localAddr, const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const {
        return loop_;
    }

    const string& name() const {
        return name_;
    }

    const InetAddress& localAddress() const {
        return localAddr_;
    }

    const InetAddress& peerAddress() const {
        return peerAddr_;
    }

    bool connected() const {
        return state_ == StateE::kConnected;
    }

    bool disconnected() const {
        return state_ == StateE::kDisconnected;
    }

    bool getTcpInfo(struct tcp_info*) const;

    string getTcpInfoString() const;

    // void send(string&& message); C++11
    void send(const void* message, int len);
    void send(const StringPiece& message);
    void send(Buffer* message);
    void shutdown();

    void forceClose();
    void forceCloseWithDelay(double seconds);
    void setTcpNoDelay(bool on);

    void startRead();
    void stopRead();

    // NOT thread safe, may race with start/stopReadInLoop
    bool isReading() const {
        return reading_;
    }

    void setContext(const boost::any& context) {
        context_ = context;
    }

    const boost::any& getContext() const {
        return context_;
    }

    boost::any* getMutableContext() {
        return &context_;
    }

    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb,
                                  size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    Buffer* inputBuffer() {
        return &inputBuffer_;
    }

    Buffer* outputBuffer() {
        return &outputBuffer_;
    }

    void setCloseCallback(const CloseCallback& cb) {
        closeCallback_ = cb;
    }

    /// called when TcpServer accepts a new connection
    ///
    /// should be called only once
    void connectEstablished();

    /// called when TcpServer has removed me from its map
    ///
    /// should be called only once
    void connectDestroyed();

private:
    enum class StateE {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(const StringPiece& message);
    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();
    void setState(StateE s) {
        state_ = s;
    }
    const char* stateToString() const;
    void startReadInLoop();
    void stopReadInLoop();

    EventLoop* loop_;
    const string name_;
    StateE state_;
    bool reading_;
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;
    boost::any context_;
};

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

}  // namespace net
}  // namespace fishnet
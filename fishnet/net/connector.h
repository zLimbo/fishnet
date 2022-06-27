#pragma once

#include <functional>
#include <memory>

#include "fishnet/base/noncopyable.h"
#include "fishnet/net/inet_address.h"

namespace fishnet {

namespace net {

class Channel;
class EventLoop;

class Connector : noncopyable, public std::enable_shared_from_this<Connector> {
public:
    using NewConnectionCallback = std::function<void(int sockfd)>;

    Connector(EventLoop* loop, const InetAddress& serverAddr);
    ~Connector();

    void setNewConnectionCallback(const NewConnectionCallback& cb) {
        newConnectionCallback_ = cb;
    }

    // can be called in any thread
    void start();

    // must be called in loop thread
    void restart();

    // can be called in any thread
    void stop();

    const InetAddress& serverAddress() const {
        return serverAddr_;
    }

private:
    enum class States { kDisconnected, kConnecting, kConnected };
    static const int kMaxRetryDelayMs = 30 * 1000;
    static const int kInitRetryDelayMs = 500;

    void setState(States s) {
        state_ = s;
    }

    void startInLoop();

    void stopInLoop();

    void connect();

    void connecting(int sockfd);

    void handleWrite();

    void handleError();

    void retry(int sockfd);

    int removeAndResetChannel();

    void resetChannel();

    EventLoop* loop_;
    InetAddress serverAddr_;
    bool connect_;
    States state_;
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback_;
    int retryDelaysMs_;
};
}  // namespace net

}  // namespace fishnet
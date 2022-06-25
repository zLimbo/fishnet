#pragma once

#include "fishnet/base/noncopyable.h"
#include "fishnet/net/channel.h"
#include "fishnet/net/socket.h"

namespace fishnet {

namespace net {

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
public:
    using NewConnectionCallback =
        std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb) {
        newConnectionCallback_ = cb;
    }

    void listen();

    bool listening() const {
        return listening_;
    }

private:
    void handleRead();
    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
    int idleFd_;
};

}  // namespace net
}  // namespace fishnet
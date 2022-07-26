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
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listen_addr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb) {
        new_connection_callback_ = cb;
    }

    void listen();

    bool listening() const {
        return listening_;
    }

private:
    void handleRead();
    EventLoop* loop_;
    Socket accept_socket_;
    Channel accept_channel_;
    NewConnectionCallback new_connection_callback_;
    bool listening_;
    int idle_fd_;
};

}  // namespace net
}  // namespace fishnet
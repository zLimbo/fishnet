#pragma once

#include "fishnet/base/noncopyable.h"

// in <netinet/tcp.h>
struct tcp_info;

namespace fishnet {

namespace net {

class InetAddress;

// 对socket的封装，线程安全，在析构时关闭fd
class Socket : noncopyable {
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}

    // Socket(Socket&&) 移动构造
    ~Socket();

    int fd() const {
        return sockfd_;
    }

    // return true if sucess
    bool getTcpInfo(struct tcp_info*) const;
    bool getTcpInfoString(char* buf, int len) const;

    /// abort if address in use
    void bindAddress(const InetAddress& localaddr);
    /// abort if address in use
    void listen();

    // 成功则返回接受socket的fd，被设置为non-blocking和close-on-exec
    // peeraddr被赋予地址；失败返回-1
    int accept(InetAddress* peeraddr);

    void shutdownWrite();

    /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm)
    void setTcpNoDelay(bool on);

    /// Enable/disable SO_REUSEADDR
    void setReuseAddr(bool on);

    /// Enable/disable SO_REUSEPORT
    void setReusePort(bool on);

    /// Enable/disable SO_KEEPALIVE
    void setKeepAlive(bool on);

private:
    const int sockfd_;
};
}  // namespace net
}  // namespace fishnet
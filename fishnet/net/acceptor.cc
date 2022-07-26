#include "fishnet/net/acceptor.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>

#include "fishnet/base/logging.h"
#include "fishnet/net/event_loop.h"
#include "fishnet/net/inet_address.h"
#include "fishnet/net/sockets_ops.h"

using namespace fishnet;
using namespace fishnet::net;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listen_addr, bool reuseport)
    : loop_(loop),
      accept_socket_(sockets::createNonblockingOrDie(listen_addr.family())),
      accept_channel_(loop, accept_socket_.fd()),
      listening_(false),
      idle_fd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
    assert(idle_fd_ >= 0);
    accept_socket_.setReuseAddr(true);
    accept_socket_.setReusePort(reuseport);
    accept_socket_.bindAddress(listen_addr);
    accept_channel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    accept_channel_.disableAll();
    accept_channel_.remove();
    ::close(idle_fd_);
}

void Acceptor::listen() {
    loop_->assertInLoopThread();
    listening_ = true;
    accept_socket_.listen();
    accept_channel_.enableReading();
}

void Acceptor::handleRead() {
    loop_->assertInLoopThread();
    InetAddress peer_addr;
    int connfd = accept_socket_.accept(&peer_addr);
    if (connfd >= 0) {
        if (new_connection_callback_) {
            // 接受连接后加入后续监听epoll中？
            new_connection_callback_(connfd, peer_addr);
        } else {
            // 没有回调函数就关闭连接？
            sockets::close(connfd);
        }
    } else {
        // 当调用 accept 函数接受客户端连接，函数返回失败，对应的错误码是 EMFILE,
        // 它表示当前进程打开的文件描述符已达上限，此时，服务器不能再接受客户端连接
        // 在 poll/epoll 中会存在问题，因为fd不够事件一直无法得到处理：
        // 水平触发会一直唤醒epoll，边沿触发则会一直忽略后面的连接
        // 解决办法：
        // 1、事先准备一个空闲的文件描述符 idlefd，相当于先占一个"坑"位
        // 2、调用 close 关闭 idlefd，关闭之后，进程就会获得一个文件描述符名额
        // 3、再次调用 accept 函数, 此时就会返回新的文件描述符 clientfd, 立刻调用 close 函数，
        //    关闭 clientfd
        // 4、重新创建空闲文件描述符 idlefd，重新占领 "坑" 位，再出现这种情况的时候又可以使用
        LOG_SYSERR << "in Acceptor::handleRead";
        if (errno == EMFILE) {
            ::close(idle_fd_);
            idle_fd_ = ::accept(accept_socket_.fd(), nullptr, nullptr);
            ::close(idle_fd_);
            idle_fd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}
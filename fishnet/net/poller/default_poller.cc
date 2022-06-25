#include <cstdlib>

#include "fishnet/net/poller.h"
#include "fishnet/net/poller/epoll_poller.h"
#include "fishnet/net/poller/poll_poller.h"

using namespace fishnet::net;

Poller* Poller::newDefaultPoller(EventLoop* loop) {
    if (::getenv("FISHNET_USE_POLL")) {
        return new PollPoller(loop);
    } else {
        return new EPollPoller(loop);
    }
}
#pragma once

#include <vector>

#include "fishnet/base/timestamp.h"
#include "fishnet/net/channel.h"
#include "fishnet/net/event_loop.h"
#include "fishnet/net/poller.h"

struct epoll_event;

namespace fishnet {

namespace net {

class EPollPoller : public Poller {
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;

    void updateChannel(Channel* channel) override;

    void removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16;

    static const char* operationToString(int op);

    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

    void update(int operation, Channel* channel);

    using EventList = std::vector<struct epoll_event>;

    int epollfd_;
    EventList events_;
};
}  // namespace net
}  // namespace fishnet
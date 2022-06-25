#pragma once

#include <vector>

#include "fishnet/base/timestamp.h"
#include "fishnet/net/event_loop.h"
#include "fishnet/net/poller.h"

struct pollfd;

namespace fishnet {

namespace net {

class PollPoller : public Poller {
public:
    PollPoller(EventLoop* loop);

    ~PollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;

    void updateChannel(Channel* channel) override;

    void removeChannel(Channel* channel) override;

private:
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

    using PollFdList = std::vector<struct pollfd>;

    PollFdList pollfds_;
};

}  // namespace net
}  // namespace fishnet
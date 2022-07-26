#pragma once

#include <map>
#include <vector>

#include "fishnet/base/noncopyable.h"
#include "fishnet/base/timestamp.h"
#include "fishnet/net/event_loop.h"

namespace fishnet {

namespace net {

class Channel;

///
/// Base class for IO Multiplexing
///
/// This class doesn't own the Channel objects.
class Poller : noncopyable {
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    
    virtual ~Poller();

    /// Polls the I/O events.
    /// Must be called in the loop thread.
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;

    /// Changes the interested I/O events.
    /// Must be called in the loop thread.
    virtual void updateChannel(Channel* channel) = 0;

    virtual void removeChannel(Channel* channel) = 0;

    virtual bool hasChannel(Channel* channel) const;

    static Poller* newDefaultPoller(EventLoop* loop);

    void assertInLoopThread() const {
        ownerLoop_->assertInLoopThread();
    }

protected:
    using ChannelMap = std::map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop* ownerLoop_;
};

}  // namespace net
}  // namespace fishnet
#pragma once

#include "fishnet/base/condition.h"
#include "fishnet/base/mutex.h"
#include "fishnet/base/noncopyable.h"
#include "fishnet/base/thread.h"
#include "fishnet/net/event_loop.h"

namespace fishnet {

namespace net {

class EventLoop;

class EventLoopThread : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                    const string& name = string());

    ~EventLoopThread();

    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_ GUARDED_BY(mutex_);
    bool exiting_;
    Thread thread_;
    MutexLock mutex_;
    Condition cond_ GUARDED_BY(mutex_);
    ThreadInitCallback callback_;
};
}  // namespace net
}  // namespace fishnet
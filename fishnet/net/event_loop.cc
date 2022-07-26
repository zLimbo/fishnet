#include "fishnet/net/event_loop.h"

#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>

#include "fishnet/base/logging.h"
#include "fishnet/base/mutex.h"
#include "fishnet/base/timestamp.h"
#include "fishnet/net/callbacks.h"
#include "fishnet/net/channel.h"
#include "fishnet/net/poller.h"
#include "fishnet/net/sockets_ops.h"
#include "fishnet/net/timer_queue.h"

using namespace fishnet;
using namespace fishnet::net;

namespace {

__thread EventLoop* t_loopInThisThread = 0;

const int kPollTimeMs = 10000;

int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_SYSERR << "Failed in eventfd";
        abort();
    }
    return evtfd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"

class IgnoreSigPipe {
public:
    IgnoreSigPipe() {
        ::signal(SIGPIPE, SIG_IGN);
    }
};

#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;
}  // namespace

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      eventHanding_(false),
      callingPendingFunctors_(false),
      iteration_(0),
      threadId_(current_thread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      timerQueue_(new TimerQueue{this}),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel{this, wakeupFd_}),
      currentActiveChannel_(NULL) {
    LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
    if (t_loopInThisThread) {
        LOG_FATAL << "Another EventLoop " << t_loopInThisThread << " exists in this thread "
                  << threadId_;
    } else {
        t_loopInThisThread = this;
    }
    //
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 注册到loop.poller上
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_ << " destructs in thread "
              << current_thread::tid();
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = NULL;
}

void EventLoop::loop() {
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;
    LOG_TRACE << "EventLoop " << this << " start looping";

    while (!quit_) {
        activeChannels_.clear();
        // 每10ms超时唤醒
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        ++iteration_;
        if (Logger::LogLevel() <= Logger::LogLevel::TRACE) {
            printActiveChannels();
        }
        eventHanding_ = true;
        for (Channel* channel : activeChannels_) {
            currentActiveChannel_ = channel;
            // 处理事件
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }
        currentActiveChannel_ = NULL;
        eventHanding_ = false;
        doPendingFunctors();
    }
    LOG_TRACE << "EventLoop " << this << " stop looping";
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;

    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        MutexLockGuard lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

size_t EventLoop::queueSize() const {
    MutexLockGuard lock(mutex_);
    return pendingFunctors_.size();
}

TimerId EventLoop::runAt(Timestamp time, TimerCallback cb) {
    return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallback cb) {
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb) {
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancel(TimerId timerId) {
    return timerQueue_->cancel(timerId);
}

void EventLoop::updateChannel(Channel* channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    if (eventHanding_) {
        assert(currentActiveChannel_ == channel ||
               std::find(activeChannels_.begin(), activeChannels_.end(), channel) ==
                   activeChannels_.end());
    }
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    return poller_->hasChannel(channel);
}

void EventLoop::abortNotInLoopThread() {
    LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
              << " was created in threadId_ " << threadId_
              << ", current thread id = " << current_thread::tid();
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = sockets::write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = sockets::read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    // 节省锁的使用
    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor& functor : functors) {
        functor();
    }
    callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const {
    for (const Channel* channel : activeChannels_) {
        LOG_TRACE << "{" << channel->reventsToString() << "} ";
    }
}

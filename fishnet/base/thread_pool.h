#pragma once

#include <deque>
#include <vector>

#include "fishnet/base/condition.h"
#include "fishnet/base/mutex.h"
#include "fishnet/base/thread.h"
#include "fishnet/base/types.h"

namespace fishnet {

class ThreadPool : noncopyable {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(const string& nameArg = string("ThreadPool"));
    ~ThreadPool();

    // Must be called before start()
    void setMaxQueueSize(int maxSize) {
        maxQueueSize_ = maxSize;
    }
    void setThreadInitCallback(const Task& cb) {
        threadInitCallback_ = cb;
    }

    void start(int numThreads);
    void stop();

    const string& name() const {
        return name_;
    }

    size_t queueSize() const;

    void run(Task f);

private:
    bool isFull() const REQUIRES(mutex_);
    void runInThread();
    Task take();

    mutable MutexLock mutex_;
    Condition notEmpty_ GUARDED_BY(mutex_);
    Condition notFull_ GUARDED_BY(mutex_);
    string name_;
    Task threadInitCallback_;
    std::vector<std::unique_ptr<fishnet::Thread>> threads_;
    std::deque<Task> queue_ GUARDED_BY(mutex_);
    size_t maxQueueSize_;
    bool running_;
};

}  // namespace fishnet
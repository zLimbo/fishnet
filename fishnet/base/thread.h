#pragma once

#include "fishnet/base/atomic.h"
#include "fishnet/base/countdown_latch.h"
#include "fishnet/base/types.h"

#include <functional>
#include <memory>
#include <pthread.h>

namespace fishnet {
class Thread : noncopyable {
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const string& name = string());

    ~Thread();

    void start();

    // 返回 pthread_join()
    int join();

    bool started() const { return started_; }

private:
    void setDefaultName();
    bool started_;
    bool joined_;
    pthread_t pthreadId_;
    pid_t tid_;
    ThreadFunc func_;
    string name_;
    CountDownLatch latch_;

    static AtomicInt32 numCreated_;
};

}  // namespace fishnet
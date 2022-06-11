#pragma once

#include "fishnet/base/condition.h"
#include "fishnet/base/mutex.h"

namespace fishnet {
class CountDownLatch : noncopyable {
public:
    explicit CountDownLatch(int count);

    void wait();

    void countDown();

    int getCount() const;

private:
    mutable MutexLock mutex_;
    Condition cond_ GUARDED_BY(mutex_);
    int count_ GUARDED_BY(mutex_);
};
}  // namespace fishnet
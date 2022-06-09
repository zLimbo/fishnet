#pragma once

#include "fishnet/base/condition.h"
#include "fishnet/base/mutex.h"

namespace fishnet {
class CountdownLatch : noncopyable {
public:
    explicit CountdownLatch(int count);

    void wait();

    void countDown();

    int getCount() const;

private:
    mutable MutexLock mutex_;
    Condition cond_ GUARDED_BY(mutex_);
    int count_ GUARDED_BY(mutex_);
};
}  // namespace fishnet
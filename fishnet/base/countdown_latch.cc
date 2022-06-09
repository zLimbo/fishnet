#include "fishnet/base/countdown_latch.h"

using namespace fishnet;

CountdownLatch::CountdownLatch(int count)
    : mutex_(), cond_(mutex_), count_(count) {}

void CountdownLatch::wait() {
    MutexLockGuard lock(mutex_);
    while (count_ > 0) {
        cond_.wait();
    }
}

void CountdownLatch::countDown() {
    MutexLockGuard lock(mutex_);
    --count_;
    if (count_ == 0) {
        cond_.notifyAll();
    }
}

int CountdownLatch::getCount() const {
    MutexLockGuard lock(mutex_);
    return count_;
}
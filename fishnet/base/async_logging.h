#pragma once

#include <atomic>
#include <vector>

#include "fishnet/base/blocking_queue.h"
#include "fishnet/base/bounded_blocking_queue.h"
#include "fishnet/base/countdown_latch.h"
#include "fishnet/base/log_stream.h"
#include "fishnet/base/mutex.h"
#include "fishnet/base/thread.h"

namespace fishnet {

class AsyncLogging : noncopyable {
public:
    AsyncLogging(const string& basename, off_t rollSize, int flushInterval = 3);

    ~AsyncLogging() {
        if (running_) {
            stop();
        }
    }

    void append(const char* logline, int len);

    void start() {
        running_ = true;
        thread_.start();
        latch_.wait();
    }

    void stop() NO_THREAD_SAFETY_ANALYSIS {
        running_ = false;
        cond_.notify();
        thread_.join();
    }

private:
    void threadFunc();

    using Buffer = fishnet::detail::FixedBuffer<fishnet::detail::kLargerBuffer>;
    using BufferVector = std::vector<std::unique_ptr<Buffer>>;
    using BufferPtr = BufferVector::value_type;

    const int flushInterval_;
    std::atomic<bool> running_;
    const string basename_;
    const off_t rollSize_;
    Thread thread_;
    CountDownLatch latch_;
    MutexLock mutex_;
    Condition cond_ GUARDED_BY(mutex_);
    BufferPtr currentBuffer_ GUARDED_BY(mutex_);
    BufferPtr nextBuffer_ GUARDED_BY(mutex_);
    BufferVector buffers_ GUARDED_BY(mutex_);
};

}  // namespace fishnet
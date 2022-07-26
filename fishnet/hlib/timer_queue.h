#ifndef TIMER_QUEUE_H
#define TIMER_QUEUE_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

namespace zfish {

class TimerQueue {
public:
    using HighClock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<HighClock>;
    
    struct Timer {
        TimePoint time_point;
        std::function<void()> func;
        int repeated_id;
        bool operator<(const Timer& rhs) const {
            return time_point > rhs.time_point;
        }
    };

    using TimerPtr = std::unique_ptr<Timer>;

    template <typename Ratio, typename Period, typename Func, typename... Args>
    void RunAfter(const std::chrono::duration<Ratio, Period>& duration, Func&& func,
                  Args&&... args) {
        Timer timer;
        timer.time_point = HighClock::now() + duration;
        timer.func = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
        std::unique_lock<std::mutex> lock{mutex_};
        queue_.push(timer);
        cond_.notify_all();
    }

    template <typename Func, typename... Args>
    void RunAt(const TimePoint& time_point, Func&& func, Args&&... args) {
        Timer timer;
        timer.time_point = time_point;
        timer.func = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
        std::unique_lock<std::mutex> lock{mutex_};
        queue_.push(timer);
        cond_.notify_all();
    }

private:
    void Loop() {
        while (running_.load()) {
            std::unique_lock<std::mutex> lock{mutex_};
            cond_.wait(lock, [this] { return !queue_.empty(); });
            Timer timer = queue_.top();
            auto diff = timer.time_point - HighClock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() > 0) {
                cond_.wait_for(lock, diff);
            } else {
                
            }
        }
    }

private:
    std::priority_queue<Timer> queue_;

    std::atomic<bool> running_;

    std::mutex mutex_;
    std::condition_variable cond_;
};

}  // namespace zfish

#endif  // TIMER_QUEUE_H
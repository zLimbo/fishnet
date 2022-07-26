#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace zfish {

class ThreadPool {
public:
    using Milliseconds = std::chrono::milliseconds;

    struct PoolConfig {
        int core_thread_num;
        int max_thread_num;
        int max_task_num;
        Milliseconds timeout;

        PoolConfig(int core_thread_num, int max_thread_num, int max_task_num, Milliseconds timeout)
            : core_thread_num(core_thread_num),
              max_thread_num(max_thread_num),
              max_task_num(max_task_num),
              timeout(timeout) {}
    };

    enum class ThreadState { kInit, kWaiting, kRunning, kStop };

    enum class ThreadFlag { kInit, kCore, kCache };

    using ThreadUniquePtr = std::unique_ptr<std::thread>;
    using ThreadId = std::atomic<int>;
    using ThreadStateAtomic = std::atomic<ThreadState>;
    using ThreadFlagAtomic = std::atomic<ThreadFlag>;

    struct ThreadWrapper {
        ThreadUniquePtr th;
        ThreadId id;
        ThreadFlagAtomic flag;
        ThreadStateAtomic state;
    };

    using ThreadWrapperSharedPtr = std::shared_ptr<ThreadWrapper>;
    using ThreadPoolLock = std::unique_lock<std::mutex>;

    // max_thread_num = -1 表示 max_thread_num = core_thread_num
    ThreadPool(int core_thread_num, int max_thread_num = -1,
               int max_task_num = std::numeric_limits<int>::max(),
               Milliseconds timeout = Milliseconds{100})
        : ThreadPool(PoolConfig(core_thread_num,
                                max_thread_num == -1 ? core_thread_num : max_thread_num,
                                max_task_num, timeout)) {}

    ThreadPool(PoolConfig config)
        : config_(config),
          is_shutdown_(false),
          is_shutdown_now_(false),
          waiting_thread_num_(0),
          total_task_num_(0),
          next_thread_id_(0) {
        if (IsAvaliableConfig(config)) {
            is_available_.store(true);
            printf(
                "Pool: cfg: core_thread_num=%d, max_thread_num=%d, max_task_num=%d, timeout=%ld\n",
                config.core_thread_num, config.max_thread_num, config.max_task_num,
                config.timeout.count());
            Start();
        } else {
            is_available_.store(false);
        }
    }

    ~ThreadPool() {
        Shutdown();
    }

    bool IsAvaliableConfig(PoolConfig config) const {
        return config.core_thread_num > 0 && config.max_task_num > 0 &&
               config.max_task_num > config.core_thread_num && config.timeout >= Milliseconds{0};
    }

    bool IsAvailable() const {
        return is_available_.load();
    }

    bool IsShutdown() const {
        return is_shutdown_.load();
    }

    bool IsShutdownNow() const {
        return is_shutdown_now_.load();
    }

    int GetWaitingThreadNum() const {
        return waiting_thread_num_.load();
    }

    int GetTotalThreadNum() const {
        ThreadPoolLock lock{mutex_};
        return threads_.size();
    }

    int GetTotalTaskNum() const {
        return total_task_num_.load();
    }

    template <typename Func, typename... Args>
    auto Run(Func&& func, Args&&... args)
        -> std::shared_ptr<std::future<std::result_of_t<Func(Args...)>>> {
        if (!IsAvailable() || IsShutdown() || IsShutdownNow()) {
            return nullptr;
        }
        if (GetWaitingThreadNum() == 0 && GetTotalThreadNum() < config_.max_thread_num) {
            AddThread(GetNextThreadId(), ThreadFlag::kCache);
        }

        using ReturnType = std::result_of_t<Func(Args...)>;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        ++total_task_num_;
        std::future<ReturnType> res = task->get_future();
        {
            ThreadPoolLock lock{mutex_};
            tasks_.emplace([task] { (*task)(); });
        }
        cond_.notify_one();
        return std::make_shared<decltype(res)>(std::move(res));
    }

    void Shutdown() {
        Shutdown(false);
    }

    void ShutdownNow() {
        Shutdown(true);
    }

    // 弹出未执行的任务
    std::vector<std::function<void()>> PopNoRunTasks() {
        std::vector<std::function<void()>> res;
        {
            ThreadPoolLock lock{mutex_};
            while (!tasks_.empty()) {
                res.push_back(std::move(tasks_.front()));
                tasks_.pop();
            }
        }
        return res;
    }

private:
    bool Start() {
        if (!IsAvailable()) {
            return false;
        }
        printf("Pool: Start, Init core thread start, num=%d\n", config_.core_thread_num);
        for (int i = 0; i < config_.core_thread_num; ++i) {
            AddThread(GetNextThreadId(), ThreadFlag::kCore);
        }
        printf("Pool: Init core thread end\n");
        return true;
    }

    void Shutdown(bool is_now) {
        if (IsAvailable()) {
            if (is_now) {
                is_shutdown_now_.store(true);
            } else {
                is_shutdown_.store(true);
            }
            cond_.notify_all();
            is_available_.store(false);
        }

        // FIXME: join所有线程，可改进为detach
        for (auto it : threads_) {
            if (it->th->joinable()) {
                it->th->join();
                // it->th->detach();
            }
        }
        printf("Pool: shutdown is_now=%d\n", is_now);
    }

    int GetNextThreadId() {
        return next_thread_id_++;
    }

    void AddThread(int id, ThreadFlag flag) {
        printf("Pool: add thread(%d) flag=%d\n", id, static_cast<int>(flag));
        ThreadWrapperSharedPtr thread_ptr = std::make_shared<ThreadWrapper>();
        thread_ptr->id.store(id);
        thread_ptr->flag.store(flag);

        auto work_loop = [this, thread_ptr] {
            for (;;) {
                std::function<void()> task;
                {
                    ThreadPoolLock lock{mutex_};
                    if (thread_ptr->state.load() == ThreadState::kStop) {
                        break;
                    }
                    printf("Pool: thread(%d) wait start\n", thread_ptr->id.load());
                    thread_ptr->state.store(ThreadState::kWaiting);
                    ++waiting_thread_num_;

                    // 进入等待，条件判断
                    bool is_timeout = false;
                    auto wakeup_pred = [this, thread_ptr] {
                        return IsShutdown() || IsShutdownNow() || !tasks_.empty() ||
                               thread_ptr->state.load() == ThreadState::kStop;
                    };
                    if (thread_ptr->flag.load() == ThreadFlag::kCore) {
                        cond_.wait(lock, wakeup_pred);
                        printf("Pool: thread(%d) wait end\n", thread_ptr->id.load());
                    } else {
                        cond_.wait_for(lock, config_.timeout, wakeup_pred);
                        is_timeout = !wakeup_pred();
                        printf("Pool: thread(%d) wait end with timeout=%d\n", thread_ptr->id.load(),
                               static_cast<int>(is_timeout));
                    }

                    // 等待完成
                    --waiting_thread_num_;
                    if (is_timeout) {
                        thread_ptr->state.store(ThreadState::kStop);
                    }

                    // 是否终止线程判断
                    if (thread_ptr->state.load() == ThreadState::kStop) {
                        printf("Pool: thread(%d) state stop\n", thread_ptr->id.load());
                        break;
                    }
                    if (IsShutdown() && tasks_.empty()) {
                        printf("Pool: thread(%d) state shutdown\n", thread_ptr->id.load());
                        break;
                    }
                    if (IsShutdownNow()) {
                        printf("Pool: thread(%d) state shutdown now\n", thread_ptr->id.load());
                        break;
                    }

                    // 取出一个任务开始执行
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    thread_ptr->state.store(ThreadState::kRunning);
                }
                printf("Pool: thread(%d) run task\n", thread_ptr->id.load());
                task();
            }
            printf("Pool: thread(%d) state terminate\n", thread_ptr->id.load());
        };
        thread_ptr->th = std::make_unique<std::thread>(std::move(work_loop));
        // 添加到线程链表中
        {
            ThreadPoolLock lock{mutex_};
            threads_.emplace_back(std::move(thread_ptr));
        }
    }

private:
    PoolConfig config_;

    std::list<ThreadWrapperSharedPtr> threads_;
    std::queue<std::function<void()>> tasks_;

    mutable std::mutex mutex_;
    std::condition_variable cond_;

    std::atomic<bool> is_available_;
    std::atomic<bool> is_shutdown_;
    std::atomic<bool> is_shutdown_now_;

    std::atomic<int> waiting_thread_num_;
    std::atomic<int> total_task_num_;

    std::atomic<int> next_thread_id_;
};

}  // namespace zfish

#endif  // THREAD_POOL_H
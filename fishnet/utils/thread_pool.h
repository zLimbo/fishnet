#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "../base/logging.h"

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

    using ThreadPtr = std::shared_ptr<std::thread>;
    using ThreadId = std::atomic<int>;
    using ThreadStateAtomic = std::atomic<ThreadState>;
    using ThreadFlagAtomic = std::atomic<ThreadFlag>;

    struct ThreadWrapper {
        ThreadPtr th;
        ThreadId id;
        ThreadFlagAtomic flag;
        ThreadStateAtomic state;
    };

    using ThreadWrapperPtr = std::shared_ptr<ThreadWrapper>;
    using ThreadPoolLock = std::unique_lock<std::mutex>;

    ThreadPool(PoolConfig config)
        : config_(config),
          is_shutdown_(false),
          is_shutdown_now_(false),
          waiting_thread_num_(0),
          total_task_num_(0),
          next_thread_id_(0) {
        if (IsAvaliableConfig(config)) {
            is_available_.store(true);
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

    bool Start() {
        if (!IsAvailable()) {
            return false;
        }
        LOG_DEBUG << "Init core thread start, num=" << config_.core_thread_num;
        for (int i = 0; i < config_.core_thread_num; ++i) {
            AddThread(GetNextThreadId(), ThreadFlag::kCore);
        }
        LOG_DEBUG << "Init core thread end";
        return true;
    }

    template <typename Func, typename... Args>
    auto Run(Func func, Args... args)
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

private:
    void Shutdown(bool is_now) {
        LOG_DEBUG << "shutdown is_now=" << is_now;
        if (IsAvailable()) {
            if (is_now) {
                is_shutdown_now_.store(true);
            } else {
                is_shutdown_.store(true);
            }
            cond_.notify_all();
            is_available_.store(false);
        }
    }

    int GetNextThreadId() {
        return next_thread_id_++;
    }

    void AddThread(int id, ThreadFlag flag) {
        LOG_DEBUG << "add thread(" << id << ") flag=" << static_cast<int>(flag);
        ThreadWrapperPtr thread_ptr = std::make_shared<ThreadWrapper>();
        thread_ptr->id.store(id);
        thread_ptr->flag.store(flag);

        auto func = [this, thread_ptr] {
            for (;;) {
                std::function<void()> task;
                {
                    ThreadPoolLock lock{mutex_};
                    if (thread_ptr->state.load() == ThreadState::kStop) {
                        break;
                    }

                    LOG_DEBUG << "thread(" << thread_ptr->id.load() << ") wait start";
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
                    } else {
                        cond_.wait_for(lock, config_.timeout, wakeup_pred);
                        is_timeout = !wakeup_pred();
                    }

                    // 等待完成
                    LOG_DEBUG << "thread(" << thread_ptr->id.load() << ") wait end";
                    --waiting_thread_num_;
                    if (is_timeout) {
                        thread_ptr->state.store(ThreadState::kStop);
                    }

                    // 是否终止线程判断
                    if (thread_ptr->state.load() == ThreadState::kStop) {
                        LOG_DEBUG << "thread(" << thread_ptr->id.load() << ") state stop";
                        break;
                    }
                    if (IsShutdown() && tasks_.empty()) {
                        LOG_DEBUG << "thread(" << thread_ptr->id.load() << ") shutdown";
                        break;
                    }
                    if (IsShutdownNow()) {
                        LOG_DEBUG << "thread(" << thread_ptr->id.load() << ") shutdown now";
                        break;
                    }

                    // 取出一个任务开始执行
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    thread_ptr->state.store(ThreadState::kRunning);
                }
                task();
            }
            LOG_DEBUG << "thread id " << thread_ptr->id.load() << " stop";
        };
        thread_ptr->th = std::make_shared<std::thread>(std::move(func));
        if (thread_ptr->th->joinable()) {
            thread_ptr->th->detach();
        }
        // 添加到线程链表中
        {
            ThreadPoolLock lock{mutex_};
            threads_.emplace_back(std::move(thread_ptr));
        }
    }

private:
    PoolConfig config_;

    std::list<ThreadWrapperPtr> threads_;
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
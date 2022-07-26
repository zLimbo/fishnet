#ifndef ZFISH_H
#define ZFISH_H

#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace zfish {

class Util {
public:
// #if __cplusplus >= 201703L
// // #if __has_include(<string_view>)
// #include <string_view>
//     vector<string_view> split(string_view sv, string_view delim) {
//         vector<string_view> words;
//         auto b = sv.find_first_not_of(delims);
//         auto e = sv.find_first_of(delims, b);

//         while (b != string_view::npos) {
//             e = min(e, sv.size());
//             words.push_back(sv.substr(b, e - b));
//             b = sv.find_first_not_of(delims, e);
//             e = sv.find_first_of(delims, b);
//         }
//         return words;
//     }
// #elif
//     vector<string> split(const string& sv, const string& delims) {
//         vector<string> words;
//         auto b = sv.find_first_not_of(delims);
//         auto e = sv.find_first_of(delims, b);

//         while (b != string::npos) {
//             e = min(e, sv.size());
//             words.push_back(sv.substr(b, e - b));
//             b = sv.find_first_not_of(delims, e);
//             e = sv.find_first_of(delims, b);
//         }
//         return words;
//     }
// #endif
};

template <typename T>
class Singleton {
public:
    static T& getInstance() {
        static T t;
        return t;
    }

public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

protected:
    Singleton() = default;
    ~Singleton() = default;
};

template <typename T>
class BlockingQueue {
public:
    BlockingQueue(int max_size) : max_size_(max_size) {}
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    template <typename... Args>
    void enqueue(Args&&... args) {
        std::unique_lock<std::mutex> locker(mtx_);
        not_full_.wait(locker, [this] { return queue_.size() < max_size_; });
        queue_.emplace(std::forward<Args>(args)...);
        not_empty_.notify_one();
    }

    T dequeue() {
        std::unique_lock<std::mutex> locker(mtx_);
        not_empty_.wait(locker, [this] { return queue_.size() > 0; });
        T x = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return x;
    }

    size_t size() {
        std::unique_lock<std::mutex> locker(mtx_);
        return queue_.size();
    }

private:
    size_t max_size_;
    std::queue<T> queue_;
    std::mutex mtx_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

class ThreadPool {
public:
    ThreadPool(int num) : close_(false) {
        for (int i = 0; i < num; ++i) {
            std::thread th([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> locker(mutex_);
                        cond_.wait(locker, [this] {
                            return close_ || !task_queue_.empty();
                        });
                        if (close_ && task_queue_.empty()) {
                            return;
                        }
                        task = std::move(task_queue_.front());
                        task_queue_.pop();
                    }
                    task();
                }
            });
            workers_.push_back(std::move(th));
        }
    }

    template <typename Func, typename... Args>
    auto put(Func&& func, Args&&... args)
        -> std::future<typename std::result_of<Func(Args...)>::type> {
        using ResultType = typename std::result_of<Func(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<ResultType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        {
            std::unique_lock<std::mutex> locker(mutex_);
            if (close_) {
                throw std::runtime_error("thread pool has closed!");
            }
            task_queue_.push([task] { (*task)(); });
        }
        cond_.notify_one();
        return task->get_future();
    }

    ~ThreadPool() {
        close();
    }

    void close() {
        {
            std::lock_guard<std::mutex> locker(mutex_);
            if (close_)
                return;
            close_ = true;
        }
        cond_.notify_all();
        for (std::thread& worker : workers_) {
            worker.join();
        }
    }

private:
    bool close_ = true;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> task_queue_;
};

}  // namespace zfish

#endif  // ZFISH_H
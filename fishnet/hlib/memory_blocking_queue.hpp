#pragma once
#ifndef MEMBQ_H
#define MEMBQ_H

#include <condition_variable>
#include <deque>
#include <mutex>

template <typename T>
class MemoryBlockingQueue {
public:
    MemoryBlockingQueue(size_t max_memory_size = 1024)
        : max_memory_size_(max_memory_size), cur_memory_size_(0) {
        staticCheckHaveMemorySize();
    }

    template <typename... Args>
    bool enqueue(Args&&... args) {
        T x(std::forward<Args>(args)...);
        // 如果当前元素大小大于 max_memory_size，队列无法放下该元素
        if (x.memorySize() > max_memory_size_) {
            return false;
        }
        std::unique_lock<std::mutex> lock{mu_};
        not_full_.wait(lock, [this, &x] {
            return cur_memory_size_ + x.memorySize() <= max_memory_size_;
        });
        queue_.push_back(std::move(x));
        cur_memory_size_ += x.memorySize();
        not_empty_.notify_one();
        return true;
    }

    T dequeue() {
        std::unique_lock<std::mutex> lock{mu_};
        not_empty_.wait(lock, [this] { return !queue_.empty(); });
        T x = std::move(queue_.front());
        queue_.pop_front();
        cur_memory_size_ -= x.memorySize();
        not_full_.notify_one();
        return x;
    }

    size_t size() {
        std::unique_lock<std::mutex> lock{mu_};
        return queue_.size();
    }

    size_t memorySize() {
        std::unique_lock<std::mutex> lock{mu_};
        return cur_memory_size_;
    }

    size_t maxMemorySize() {
        return max_memory_size_;
    }

private:
    void staticCheckHaveMemorySize() {
        std::decay_t<decltype(reinterpret_cast<T*>(-1)->memorySize())>* p =
            nullptr;
    }

    size_t max_memory_size_;
    size_t cur_memory_size_;
    std::deque<T> queue_;
    std::mutex mu_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

#endif  // MEMBQ_H
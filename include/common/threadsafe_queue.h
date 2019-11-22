#pragma once
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

/**
 * @brief thread-safe queue allowing push and waited pop
 */
template <typename T>
class ThreadsafeQueue {
public:
    ThreadsafeQueue() = default;

    ~ThreadsafeQueue() = default;

    /**
     * @brief push an value into the end. threadsafe.
     * @param new_value the value
     */
    void Push(T new_value) {
        mu_.lock();
        queue_.push(std::move(new_value));
        mu_.unlock();
        cond_.notify_all();
    }

    void Pop() {
        mu_.lock();
        queue_.pop();
        mu_.unlock();
    }
    template <typename Duration>
    bool WaitAndPeek(T& value, const Duration& timeout_) {
        std::unique_lock<std::mutex> lk(mu_);
        auto ret =
            cond_.wait_for(lk, timeout_, [this] { return !queue_.empty(); });
        if (ret == false) {
            return false;
        }
        value = queue_.front();
        return true;
    }
    /**
     * @brief wait until pop an element from the beginning, threadsafe
     * @param value the poped value
     */
    void WaitAndPop(T* value) {
        std::unique_lock<std::mutex> lk(mu_);
        cond_.wait(lk, [this] { return !queue_.empty(); });
        *value = std::move(queue_.front());
        queue_.pop();
    }

    bool empty() {
        std::lock_guard<std::mutex> lg(mu_);
        return queue_.empty();
    }
    int size() {
        std::lock_guard<std::mutex> lg(mu_);
        return queue_.size();
    }

private:
    mutable std::mutex mu_;
    std::queue<T> queue_;
    std::condition_variable cond_;
};

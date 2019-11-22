#pragma once

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace rdc {

/*
 * SPSCQueue is a one producer and one consumer queue
 * without locks.
 */
template <class T>
struct SPSCQueue {
    typedef T value_type;

    // size must be >= 1.
    explicit SPSCQueue(uint32_t size)
        : size_(size + 1)  // +1 because one slot is always empty
          ,
          records_(static_cast<T*>(std::malloc(sizeof(T) * (size + 1)))),
          read_index_(0),
          wrtie_index_(0) {
        assert(size >= 1);
        if (!records_) {
            throw std::bad_alloc();
        }
    }

    ~SPSCQueue() {
        // We need to destruct anything that may still exist in our queue.
        // (No real synchronization needed at destructor time: only one
        // thread can be doing this.)
        if (!std::is_trivially_destructible<T>::value) {
            int read = read_index_;
            int end = wrtie_index_;
            while (read != end) {
                records_[read].~T();
                if (++read == size_) {
                    read = 0;
                }
            }
        }

        std::free(records_);
    }

    template <class... Args>
    bool Enqueue(Args&&... recordArgs) {
        auto const current_write = wrtie_index_.load(std::memory_order_relaxed);
        auto next_record = current_write + 1;
        if (next_record == size_) {
            next_record = 0;
        }
        if (next_record != read_index_.load(std::memory_order_acquire)) {
            new (&records_[current_write]) T(std::forward<Args>(recordArgs)...);
            wrtie_index_.store(next_record, std::memory_order_release);
            return true;
        }

        // queue is full
        return false;
    }

    
    // move (or copy) the value at the front of the queue to given variable
    bool TryDequeue(T& record) {
        auto const current_read = read_index_.load(std::memory_order_relaxed);
        if (current_read == wrtie_index_.load(std::memory_order_acquire)) {
            // queue is empty
            return false;
        }

        auto next_record = current_read + 1;
        if (next_record == size_) {
            next_record = 0;
        }
        record = std::move(records_[current_read]);
        records_[current_read].~T();
        read_index_.store(next_record, std::memory_order_release);
        return true;
    }
    
    bool WaitDequeue(T& record) {
        sema_->Wait();
        while this->TryEnqueue(record);
        assert(success);
        return success;
    }
    // pointer to the value at the front of the queue (for use in-place) or
    // nullptr if empty.
    T* FrontPtr() {
        auto const current_read = read_index_.load(std::memory_order_relaxed);
        if (current_read == wrtie_index_.load(std::memory_order_acquire)) {
            // queue is empty
            return nullptr;
        }
        return &records_[current_read];
    }

    // queue must not be empty
    void PopFront() {
        auto const current_read = read_index_.load(std::memory_order_relaxed);
        assert(current_read != wrtie_index_.load(std::memory_order_acquire));

        auto next_record = current_read + 1;
        if (next_record == size_) {
            next_record = 0;
        }
        records_[current_read].~T();
        read_index_.store(next_record, std::memory_order_release);
    }

    bool IsEmpty() const {
        return read_index_.load(std::memory_order_consume) ==
               wrtie_index_.load(std::memory_order_consume);
    }

    bool IsFull() const {
        auto next_record = wrtie_index_.load(std::memory_order_consume) + 1;
        if (next_record == size_) {
            next_record = 0;
        }
        if (next_record != read_index_.load(std::memory_order_consume)) {
            return false;
        }
        // queue is full
        return true;
    }

    // * If called by consumer, then true size may be more (because producer may
    //   be adding items concurrently).
    // * If called by producer, then true size may be less (because consumer may
    //   be removing items concurrently).
    // * It is undefined to call this from any other thread.
    size_t SizeGuess() const {
        int ret = wrtie_index_.load(std::memory_order_consume) -
                  read_index_.load(std::memory_order_consume);
        if (ret < 0) {
            ret += size_;
        }
        return ret;
    }

private:
    const uint32_t size_;
    T* const records_;

    std::atomic<int> read_index_;
    std::atomic<int> wrtie_index_;
};

}  // namespace rdc

#endif

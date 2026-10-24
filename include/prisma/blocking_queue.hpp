#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace prisma {

// Thread-safe FIFO queue. pop() blocks until an item arrives or the queue
// is closed. After close(), remaining items are still delivered, then pop()
// returns std::nullopt so the consumer knows it can stop.
template <typename T>
class BlockingQueue {
public:
    bool push(T item) {
        {
            std::lock_guard lock(mutex_);
            if (closed_) {
                return false;
            }
            items_.push(std::move(item));
        }
        notEmpty_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        notEmpty_.wait(lock, [this] { return closed_ || !items_.empty(); });

        if (items_.empty()) {
            return std::nullopt;
        }
        T item = std::move(items_.front());
        items_.pop();
        return item;
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        notEmpty_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::queue<T> items_;
    bool closed_{false};
};

}  // namespace prisma

#pragma once

// ホストテスト用の rtos::Semaphore 代替 (acquire / release のみ)。

#include <condition_variable>
#include <mutex>

namespace host {

class Semaphore {
public:
    explicit Semaphore(int count = 0) : count_(count) {}

    void acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return count_ > 0; });
        --count_;
    }

    void release() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++count_;
        }
        cv_.notify_one();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_;
};

}  // namespace host

#pragma once

#include <atomic>
#include <memory>

#include "platform.hpp"
#include "types.hpp"

namespace can_rpc {

namespace detail {

// client (dispatch コンテキスト) が set() し、呼び出しスレッドが await() する共有状態
template <typename T>
struct SharedState {
    Semaphore signal{0};
    std::atomic<bool> done{false};
    Result<T> result;

    void set(const Result<T>& r) {
        result = r;
        done.store(true, std::memory_order_release);
        signal.release();
    }
};

}  // namespace detail

// call() の結果を後から受け取るためのハンドル。コピー可能 (状態は共有される)。
template <typename T>
class Future {
public:
    explicit Future(std::shared_ptr<detail::SharedState<T>> state) : state_(std::move(state)) {}

    // 完了済みか (ブロックしない)
    bool is_ready() const { return state_->done.load(std::memory_order_acquire); }

    // 完了までブロックして結果を返す。完了後は何度呼んでも同じ結果を返す。
    // EventQueue の dispatch スレッド上で呼ぶとデッドロックする。
    Result<T> await() const {
        state_->signal.acquire();
        state_->signal.release();  // 他の await() / 再 await() のためにシグナルを維持する
        return state_->result;
    }

private:
    std::shared_ptr<detail::SharedState<T>> state_;
};

// await(client.call(req)) の形で書くための糖衣
template <typename T>
Result<T> await(const Future<T>& future) {
    return future.await();
}

}  // namespace can_rpc

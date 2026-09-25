#pragma once

// ホストテスト用の EventQueue 代替。時刻は advance() でのみ進む。

#include <stdint.h>

#include <chrono>
#include <functional>
#include <map>
#include <utility>

namespace host {

class EventQueue {
public:
    // 即時実行イベントを登録する (実行は run_ready() / advance() 内)
    int call(std::function<void()> fn) { return post(now_, std::move(fn)); }

    int call_in(std::chrono::milliseconds delay, std::function<void()> fn) {
        return post(now_ + delay.count(), std::move(fn));
    }

    void cancel(int id) {
        for (auto it = events_.begin(); it != events_.end(); ++it) {
            if (it->second.id == id) {
                events_.erase(it);
                return;
            }
        }
    }

    // 現在時刻で期限を迎えているイベントを全て実行する
    void run_ready() { run_until(now_); }

    // 時刻を ms 進め、期限順にイベントを実行する
    void advance(int64_t ms) {
        const int64_t target = now_ + ms;
        run_until(target);
        now_ = target;
    }

    size_t pending() const { return events_.size(); }

private:
    struct Event {
        int id;
        std::function<void()> fn;
    };

    int post(int64_t due, std::function<void()> fn) {
        const int id = next_id_++;
        events_.emplace(std::make_pair(due, order_++), Event{id, std::move(fn)});
        return id;
    }

    void run_until(int64_t target) {
        while (!events_.empty() && events_.begin()->first.first <= target) {
            auto it = events_.begin();
            if (it->first.first > now_) {
                now_ = it->first.first;
            }
            std::function<void()> fn = std::move(it->second.fn);
            events_.erase(it);
            fn();
        }
    }

    std::map<std::pair<int64_t, uint64_t>, Event> events_;
    int64_t now_ = 0;
    uint64_t order_ = 0;
    int next_id_ = 1;
};

}  // namespace host

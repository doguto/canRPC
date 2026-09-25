#pragma once

// ホストテスト用の CAN バス / ノードのモック。
// write() は同期的に他ノードの受信ハンドラを呼ぶ (ISR コンテキストの模擬)。

#include <functional>
#include <map>
#include <vector>

#include "can_rpc/can_interface.hpp"

namespace host {

class MockBus;

class MockCan : public can_rpc::CanInterface {
public:
    explicit MockCan(MockBus& bus);

    bool write(const can_rpc::CanFrame& frame) override;

    size_t attach(RxHandler handler) override {
        const size_t handle = next_handle_++;
        handlers_[handle] = std::move(handler);
        return handle;
    }

    void detach(size_t handle) override { handlers_.erase(handle); }

    void receive(const can_rpc::CanFrame& frame) {
        // ハンドラ内で attach / detach されても安全なようにコピーして呼ぶ
        auto copy = handlers_;
        for (auto& entry : copy) {
            entry.second(frame);
        }
    }

    std::vector<can_rpc::CanFrame> sent;  // write() に渡されたフレームの履歴
    int fail_next_writes = 0;             // 正の間、write() は false を返す

private:
    MockBus& bus_;
    std::map<size_t, RxHandler> handlers_;
    size_t next_handle_ = 0;
};

class MockBus {
public:
    // 戻り値 true のフレームはバス上で消失する (受信側に届かない)
    std::function<bool(const can_rpc::CanFrame&)> drop_if;

    void add(MockCan* node) { nodes_.push_back(node); }

    void deliver(MockCan* from, const can_rpc::CanFrame& frame) {
        if (drop_if && drop_if(frame)) {
            return;
        }
        for (MockCan* node : nodes_) {
            if (node != from) {
                node->receive(frame);
            }
        }
    }

private:
    std::vector<MockCan*> nodes_;
};

inline MockCan::MockCan(MockBus& bus) : bus_(bus) { bus_.add(this); }

inline bool MockCan::write(const can_rpc::CanFrame& frame) {
    sent.push_back(frame);
    if (fail_next_writes > 0) {
        --fail_next_writes;
        return false;
    }
    bus_.deliver(this, frame);
    return true;
}

}  // namespace host

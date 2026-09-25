#pragma once

#include <stdint.h>
#include <string.h>

#include <type_traits>

#include "can_interface.hpp"
#include "platform.hpp"
#include "types.hpp"

namespace can_rpc {

// RPC クライアント。
//
// スレッド規約: call() と全ての callback は EventQueue の dispatch コンテキストで実行すること。
// 受信 ISR からは EventQueue への投入のみ行うため、内部状態は dispatch コンテキストだけが更新する。
// 破棄は EventQueue が停止している (または保留イベントが無い) 状態で行うこと。
template <typename RequestPayload, typename ResponsePayload>
class CanRpcClient {
    static_assert(sizeof(RequestPayload) <= 7, "RequestPayload must fit in 7 bytes");
    static_assert(sizeof(ResponsePayload) <= 7, "ResponsePayload must fit in 7 bytes");
    static_assert(std::is_trivially_copyable<RequestPayload>::value,
                  "RequestPayload must be trivially copyable");
    static_assert(std::is_trivially_copyable<ResponsePayload>::value,
                  "ResponsePayload must be trivially copyable");

public:
    using ResponseCallback = Callback<void(const ResponsePayload&)>;
    using ErrorCallback = Callback<void(Error)>;

    CanRpcClient(CanInterface& can, EventQueue& queue, const ClientConfig& config)
        : can_(can), queue_(queue), config_(config), next_seq_(config.initial_seq) {
        rx_handle_ = can_.attach([this](const CanFrame& frame) { on_rx(frame); });
    }

    ~CanRpcClient() {
        if (rx_handle_ != CanInterface::kInvalidHandle) {
            can_.detach(rx_handle_);
        }
        cancel_timeout();
    }

    CanRpcClient(const CanRpcClient&) = delete;
    CanRpcClient& operator=(const CanRpcClient&) = delete;

    // リクエストを送信する。結果は response / error callback で通知される。
    Status call(const RequestPayload& payload) {
        if (busy_) {
            return Status::Busy;
        }

        request_frame_.id = config_.request_id;
        request_frame_.len = static_cast<uint8_t>(1 + sizeof(RequestPayload));
        request_frame_.data[0] = next_seq_;
        memcpy(&request_frame_.data[1], &payload, sizeof(RequestPayload));

        if (!can_.write(request_frame_)) {
            return Status::SendFailed;
        }

        current_seq_ = next_seq_;
        next_seq_ = static_cast<uint8_t>(next_seq_ + 1);
        busy_ = true;
        retries_left_ = config_.max_retries;
        arm_timeout();
        return Status::Ok;
    }

    void set_response_callback(ResponseCallback cb) { response_cb_ = cb; }
    void set_error_callback(ErrorCallback cb) { error_cb_ = cb; }

private:
    // ISR コンテキスト: ID の一致確認とキューへの投入のみ行う
    void on_rx(const CanFrame& frame) {
        if (frame.id != config_.response_id) {
            return;
        }
        queue_.call([this, frame]() { handle_frame(frame); });
    }

    void handle_frame(const CanFrame& frame) {
        if (!busy_ || frame.len != 1 + sizeof(ResponsePayload) || frame.data[0] != current_seq_) {
            return;  // 対象外フレームは破棄
        }

        cancel_timeout();
        busy_ = false;

        ResponsePayload response;
        memcpy(&response, &frame.data[1], sizeof(ResponsePayload));
        if (response_cb_) {
            response_cb_(response);
        }
    }

    void on_timeout(uint32_t token) {
        if (token != timeout_token_ || !busy_) {
            return;  // キャンセル済みまたは再設定済みのタイムアウト
        }
        timeout_event_ = 0;

        if (retries_left_ == 0) {
            fail(Error::Timeout);
            return;
        }

        --retries_left_;
        if (!can_.write(request_frame_)) {
            fail(Error::SendFailed);
            return;
        }
        arm_timeout();
    }

    // busy_ を解除してから通知する。callback 内から call() を再度呼べる。
    void fail(Error error) {
        busy_ = false;
        if (error_cb_) {
            error_cb_(error);
        }
    }

    void arm_timeout() {
        const uint32_t token = ++timeout_token_;
        timeout_event_ = queue_.call_in(config_.timeout, [this, token]() { on_timeout(token); });
    }

    void cancel_timeout() {
        ++timeout_token_;
        if (timeout_event_ != 0) {
            queue_.cancel(timeout_event_);
            timeout_event_ = 0;
        }
    }

    CanInterface& can_;
    EventQueue& queue_;
    ClientConfig config_;

    ResponseCallback response_cb_;
    ErrorCallback error_cb_;

    size_t rx_handle_ = CanInterface::kInvalidHandle;
    bool busy_ = false;
    uint8_t next_seq_ = 0;
    uint8_t current_seq_ = 0;
    uint8_t retries_left_ = 0;
    int timeout_event_ = 0;
    uint32_t timeout_token_ = 0;
    CanFrame request_frame_;
};

}  // namespace can_rpc

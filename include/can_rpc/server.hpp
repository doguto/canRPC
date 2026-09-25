#pragma once

#include <stdint.h>
#include <string.h>

#include <type_traits>

#include "can_interface.hpp"
#include "platform.hpp"
#include "types.hpp"

namespace can_rpc {

// RPC サーバー。
//
// スレッド規約: request handler は EventQueue の dispatch コンテキストで実行される。
// 破棄は EventQueue が停止している (または保留イベントが無い) 状態で行うこと。
template <typename RequestPayload, typename ResponsePayload>
class CanRpcServer {
    static_assert(sizeof(RequestPayload) <= 7, "RequestPayload must fit in 7 bytes");
    static_assert(sizeof(ResponsePayload) <= 7, "ResponsePayload must fit in 7 bytes");
    static_assert(std::is_trivially_copyable<RequestPayload>::value,
                  "RequestPayload must be trivially copyable");
    static_assert(std::is_trivially_copyable<ResponsePayload>::value,
                  "ResponsePayload must be trivially copyable");

public:
    using RequestHandler = Callback<ResponsePayload(const RequestPayload&)>;

    CanRpcServer(CanInterface& can, EventQueue& queue, const ServerConfig& config)
        : can_(can), queue_(queue), config_(config) {
        rx_handle_ = can_.attach([this](const CanFrame& frame) { on_rx(frame); });
    }

    ~CanRpcServer() {
        if (rx_handle_ != CanInterface::kInvalidHandle) {
            can_.detach(rx_handle_);
        }
    }

    CanRpcServer(const CanRpcServer&) = delete;
    CanRpcServer& operator=(const CanRpcServer&) = delete;

    void set_request_handler(RequestHandler handler) { handler_ = handler; }

private:
    // ISR コンテキスト: ID の一致確認とキューへの投入のみ行う
    void on_rx(const CanFrame& frame) {
        if (frame.id != config_.request_id) {
            return;
        }
        queue_.call([this, frame]() { handle_frame(frame); });
    }

    void handle_frame(const CanFrame& frame) {
        if (frame.len != 1 + sizeof(RequestPayload)) {
            return;
        }

        const uint8_t seq = frame.data[0];

        // 同一 seq の再送要求: handler を呼ばずキャッシュ済みの応答を再送する
        if (has_last_ && seq == last_seq_) {
            can_.write(last_response_frame_);
            return;
        }

        if (!handler_) {
            return;
        }

        RequestPayload request;
        memcpy(&request, &frame.data[1], sizeof(RequestPayload));
        const ResponsePayload response = handler_(request);

        last_response_frame_.id = config_.response_id;
        last_response_frame_.len = static_cast<uint8_t>(1 + sizeof(ResponsePayload));
        last_response_frame_.data[0] = seq;
        memcpy(&last_response_frame_.data[1], &response, sizeof(ResponsePayload));
        last_seq_ = seq;
        has_last_ = true;

        can_.write(last_response_frame_);
    }

    CanInterface& can_;
    EventQueue& queue_;
    ServerConfig config_;

    RequestHandler handler_;

    size_t rx_handle_ = CanInterface::kInvalidHandle;
    bool has_last_ = false;
    uint8_t last_seq_ = 0;
    CanFrame last_response_frame_;
};

}  // namespace can_rpc

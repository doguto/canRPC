#pragma once

#include <stdint.h>

#include <chrono>
#include <utility>

#include "client.hpp"
#include "mbed_can_interface.hpp"
#include "platform.hpp"
#include "server.hpp"
#include "types.hpp"

namespace can_rpc {

// CAN・EventQueue・dispatch スレッドをまとめて保持し、client / server を生成する。
//
//   can_rpc::Node node(PA_11, PA_12, 1000000);  // rd, td, bitrate
//   auto client = node.client<AddRequest, AddResponse>(0x300, 0x301);
//   auto server = node.server<AddRequest, AddResponse>(0x300, 0x301, handler);
//
// コンストラクタで dispatch スレッドを起動する。client / server の callback と
// server の request handler はこのスレッドで実行される。
// client.call() は dispatch スレッド以外 (main など) から呼ぶこと。
//
// EventQueue とスレッドの stack は Node 内に静的に確保するため、Node をグローバルに置けば
// RAM 使用量はリンク時に確定する。サイズはテンプレート引数で変更できる。
// client() / server() は戻り値をそのまま変数に受ける (C++17 のコピー省略が必要)。
template <size_t QueueSize = 1024, size_t StackSize = 1536, size_t MaxHandlers = 4>
class BasicNode {
public:
    BasicNode(PinName rd, PinName td, int hz, osPriority priority = osPriorityNormal)
        : can_(rd, td, hz),
          queue_(QueueSize, queue_buffer_),
          thread_(priority, StackSize, stack_, "can_rpc") {
        const osStatus status = thread_.start([this]() { queue_.dispatch_forever(); });
        if (status != osOK) {
            MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, MBED_ERROR_CODE_THREAD_CREATE_FAILED),
                       "can_rpc: dispatch thread start failed");
        }
    }

    ~BasicNode() {
        queue_.break_dispatch();
        thread_.join();
    }

    BasicNode(const BasicNode&) = delete;
    BasicNode& operator=(const BasicNode&) = delete;

    template <typename RequestPayload, typename ResponsePayload>
    CanRpcClient<RequestPayload, ResponsePayload> client(const ClientConfig& config) {
        return CanRpcClient<RequestPayload, ResponsePayload>(can_, queue_, config);
    }

    template <typename RequestPayload, typename ResponsePayload>
    CanRpcClient<RequestPayload, ResponsePayload> client(uint32_t request_id, uint32_t response_id,
                                                         std::chrono::milliseconds timeout = std::chrono::milliseconds(100),
                                                         uint8_t max_retries = 3) {
        ClientConfig config{request_id, response_id};
        config.timeout = timeout;
        config.max_retries = max_retries;
        return CanRpcClient<RequestPayload, ResponsePayload>(can_, queue_, config);
    }

    // handler は dispatch スレッドで実行される
    template <typename RequestPayload, typename ResponsePayload, typename Handler>
    CanRpcServer<RequestPayload, ResponsePayload> server(uint32_t request_id, uint32_t response_id, Handler handler) {
        return CanRpcServer<RequestPayload, ResponsePayload>(
            can_, queue_, ServerConfig{request_id, response_id},
            typename CanRpcServer<RequestPayload, ResponsePayload>::RequestHandler(std::move(handler)));
    }

    // handler は後から set_request_handler() で設定する
    template <typename RequestPayload, typename ResponsePayload>
    CanRpcServer<RequestPayload, ResponsePayload> server(uint32_t request_id, uint32_t response_id) {
        return CanRpcServer<RequestPayload, ResponsePayload>(can_, queue_, ServerConfig{request_id, response_id});
    }

    // 他の用途 (独自の CAN 受信、定期処理など) で直接使う場合に
    BasicMbedCanInterface<MaxHandlers>& can() { return can_; }
    EventQueue& queue() { return queue_; }

private:
    BasicMbedCanInterface<MaxHandlers> can_;
    alignas(8) unsigned char queue_buffer_[QueueSize];
    EventQueue queue_;
    alignas(8) unsigned char stack_[StackSize];
    rtos::Thread thread_;
};

using Node = BasicNode<>;

}  // namespace can_rpc

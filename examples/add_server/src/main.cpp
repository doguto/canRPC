#include "mbed.h"
#include "can.hpp"  // Step::CAN (robostep-libs-cpp)
#include "can_rpc/can_rpc.hpp"
#include "can_rpc/mbed_can_interface.hpp"
#include "add_payload.hpp"

// NUCLEO-F303K8: D10=PA_11(RD) / D2=PA_12(TD)
#define CAN_RD_PIN PA_11
#define CAN_TD_PIN PA_12
#define EVENT_QUEUE_SIZE (2 * 1024)  // RAM 16KB のため縮小
#define CAN_BITRATE 1000000

// === グローバルオブジェクト ===
Step::CAN can(CAN_RD_PIN, CAN_TD_PIN, CAN_BITRATE);
can_rpc::MbedCanInterface<Step::CAN> can_interface(can);
events::EventQueue queue(EVENT_QUEUE_SIZE);
can_rpc::CanRpcServer<AddRequest, AddResponse> add_server(
    can_interface,
    queue,
    can_rpc::ServerConfig{ADD_REQUEST_ID, ADD_RESPONSE_ID});

// === 関数宣言 ===
AddResponse handleAddRequest(const AddRequest &request);

int main()
{
    printf("[SERVER] START\r\n");

    add_server.set_request_handler(handleAddRequest);

    queue.dispatch_forever();
}

// EventQueue の dispatch コンテキストで実行される
AddResponse handleAddRequest(const AddRequest &request)
{
    AddResponse response{};
    response.sum = static_cast<int32_t>(request.a) + request.b;

    printf("[SERVER] REQ a=%d b=%d -> sum=%ld\r\n", request.a, request.b, static_cast<long>(response.sum));
    return response;
}

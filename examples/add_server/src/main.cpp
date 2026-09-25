#include "mbed.h"
#include "can.hpp"  // Step::CAN (robostep-libs-cpp)
#include "can_rpc/can_rpc.hpp"
#include "can_rpc/mbed_can_interface.hpp"
#include "add_payload.hpp"

#define CAN_RD_PIN PB_8
#define CAN_TD_PIN PB_9
#define CAN_BITRATE 1000000

// === グローバルオブジェクト ===
Step::CAN can(CAN_RD_PIN, CAN_TD_PIN, CAN_BITRATE);
can_rpc::MbedCanInterface<Step::CAN> can_interface(can);
events::EventQueue queue(4 * 1024);
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

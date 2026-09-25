#include "mbed.h"
#include "can.hpp"  // Step::CAN (robostep-libs-cpp)
#include "can_rpc/can_rpc.hpp"
#include "can_rpc/mbed_can_interface.hpp"
#include "add_payload.hpp"

#define CAN_RD_PIN PB_8
#define CAN_TD_PIN PB_9
#define CAN_BITRATE 1000000
#define REQUEST_INTERVAL 1s
#define RPC_TIMEOUT 100ms
#define RPC_MAX_RETRIES 3

// === グローバルオブジェクト ===
Step::CAN can(CAN_RD_PIN, CAN_TD_PIN, CAN_BITRATE);
can_rpc::MbedCanInterface<Step::CAN> can_interface(can);
events::EventQueue queue(4 * 1024);
can_rpc::CanRpcClient<AddRequest, AddResponse> add_client(
    can_interface,
    queue,
    can_rpc::ClientConfig{ADD_REQUEST_ID, ADD_RESPONSE_ID, RPC_TIMEOUT, RPC_MAX_RETRIES});

int16_t counter = 0;

// === 関数宣言 ===
void sendRequest();
void receiveResponse(const AddResponse &response);
void receiveError(can_rpc::Error error);

int main()
{
    printf("[CLIENT] START\r\n");

    add_client.set_response_callback(receiveResponse);
    add_client.set_error_callback(receiveError);

    // call() と callback は EventQueue の dispatch コンテキストで実行する
    queue.call_every(REQUEST_INTERVAL, sendRequest);
    queue.dispatch_forever();
}

void sendRequest()
{
    const AddRequest request{counter, static_cast<int16_t>(counter * 2)};
    const can_rpc::Status status = add_client.call(request);

    printf("[CLIENT] REQ a=%d b=%d status=%d\r\n", request.a, request.b, static_cast<int>(status));
    if (status == can_rpc::Status::Ok)
    {
        counter++;
    }
}

void receiveResponse(const AddResponse &response)
{
    printf("[CLIENT] RES sum=%ld\r\n", static_cast<long>(response.sum));
}

void receiveError(can_rpc::Error error)
{
    printf("[CLIENT] ERR code=%d\r\n", static_cast<int>(error));
}

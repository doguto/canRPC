#include "mbed.h"
#include "can_rpc/can_rpc.hpp"
#include "add_payload.hpp"

// NUCLEO-F303K8: D10=PA_11(RD) / D2=PA_12(TD)
#define CAN_RD_PIN PA_11
#define CAN_TD_PIN PA_12
#define CAN_BITRATE 1000000
#define REQUEST_INTERVAL 1s
#define RPC_TIMEOUT 100ms
#define RPC_MAX_RETRIES 3

// === グローバルオブジェクト ===
// CAN / EventQueue / dispatch スレッドは Node が保持する
can_rpc::Node node(CAN_RD_PIN, CAN_TD_PIN, CAN_BITRATE);
auto add_client = node.client<AddRequest, AddResponse>(ADD_REQUEST_ID, ADD_RESPONSE_ID, RPC_TIMEOUT, RPC_MAX_RETRIES);

int main()
{
    printf("[CLIENT] START\r\n");

    int16_t counter = 0;
    while (true)
    {
        const AddRequest request{counter, static_cast<int16_t>(counter * 2)};
        const can_rpc::Result<AddResponse> result = await(add_client.call(request));

        if (result)
        {
            printf("[CLIENT] REQ a=%d b=%d -> sum=%ld\r\n", request.a, request.b, static_cast<long>(result.value().sum));
            counter++;
        }
        else
        {
            printf("[CLIENT] ERR a=%d b=%d code=%d\r\n", request.a, request.b, static_cast<int>(result.error()));
        }

        ThisThread::sleep_for(REQUEST_INTERVAL);
    }
}

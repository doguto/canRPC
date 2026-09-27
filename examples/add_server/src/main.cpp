#include "mbed.h"
#include "can_rpc/can_rpc.hpp"
#include "add_payload.hpp"

// NUCLEO-F303K8: D10=PA_11(RD) / D2=PA_12(TD)
#define CAN_RD_PIN PA_11
#define CAN_TD_PIN PA_12
#define CAN_BITRATE 1000000

// === 関数宣言 ===
AddResponse handleAddRequest(const AddRequest &request);

// === グローバルオブジェクト ===
// CAN / EventQueue / dispatch スレッドは Node が保持する。
// handler 内で printf するため dispatch スレッドの stack を広げる (QueueSize, StackSize)
can_rpc::BasicNode<1024, 2048> node(CAN_RD_PIN, CAN_TD_PIN, CAN_BITRATE);
auto add_server = node.server<AddRequest, AddResponse>(ADD_REQUEST_ID, ADD_RESPONSE_ID, handleAddRequest);

int main()
{
    printf("[SERVER] START\r\n");

    // 処理は Node の dispatch スレッドで行われるため、main は待機するだけ
    while (true)
    {
        ThisThread::sleep_for(1s);
    }
}

// Node の dispatch スレッドで実行される
AddResponse handleAddRequest(const AddRequest &request)
{
    AddResponse response{};
    response.sum = static_cast<int32_t>(request.a) + request.b;

    printf("[SERVER] REQ a=%d b=%d -> sum=%ld\r\n", request.a, request.b, static_cast<long>(response.sum));
    return response;
}

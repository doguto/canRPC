#include "mbed.h"
#include "can.hpp"  // Step::CAN (robostep-libs-cpp)
#include "can_rpc/can_rpc.hpp"
#include "can_rpc/mbed_can_interface.hpp"
#include "add_payload.hpp"

// NUCLEO-F303K8: D10=PA_11(RD) / D2=PA_12(TD)
#define CAN_RD_PIN PA_11
#define CAN_TD_PIN PA_12
// RAM が少ないため、EventQueue と dispatch スレッドの stack は静的に確保する。
// heap 任せにすると不足時に Thread::start() が黙って失敗するため、リンク時の RAM 使用量で確認できるようにする。
#define EVENT_QUEUE_SIZE 1024
#define DISPATCH_STACK_SIZE 1536
#define CAN_BITRATE 1000000
#define REQUEST_INTERVAL 1s
#define RPC_TIMEOUT 100ms
#define RPC_MAX_RETRIES 3

// === グローバルオブジェクト ===
Step::CAN can(CAN_RD_PIN, CAN_TD_PIN, CAN_BITRATE);
can_rpc::MbedCanInterface<Step::CAN> can_interface(can);
static unsigned char queue_buffer[EVENT_QUEUE_SIZE];
static uint64_t dispatch_stack[DISPATCH_STACK_SIZE / sizeof(uint64_t)];  // 8 byte アラインを確保
events::EventQueue queue(EVENT_QUEUE_SIZE, queue_buffer);
Thread dispatch_thread(osPriorityNormal, DISPATCH_STACK_SIZE, reinterpret_cast<unsigned char *>(dispatch_stack));
can_rpc::CanRpcClient<AddRequest, AddResponse> add_client(
    can_interface,
    queue,
    can_rpc::ClientConfig{ADD_REQUEST_ID, ADD_RESPONSE_ID, RPC_TIMEOUT, RPC_MAX_RETRIES});

int main()
{
    printf("[CLIENT] START\r\n");

    // EventQueue の dispatch は専用スレッドで行い、main スレッドは await でブロックする
    const osStatus status = dispatch_thread.start([]() { queue.dispatch_forever(); });
    if (status != osOK)
    {
        printf("[CLIENT] ERR dispatch thread start failed status=%d\r\n", static_cast<int>(status));
        return 1;
    }

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

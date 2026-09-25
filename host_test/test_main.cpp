// CanRpc ホストテスト (簡易ハーネス)。失敗が 1 件でもあれば終了コード 1 を返す。

#include <stdint.h>
#include <stdio.h>

#include <chrono>
#include <vector>

#include "can_rpc/can_rpc.hpp"
#include "mock_can.hpp"

using namespace std::chrono_literals;
using can_rpc::CanFrame;
using can_rpc::ClientConfig;
using can_rpc::Error;
using can_rpc::ServerConfig;
using can_rpc::await;
using can_rpc::Status;

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        ++g_checks;                                                     \
        if (!(cond)) {                                                  \
            ++g_failures;                                               \
            printf("[FAIL] %s:%d %s\n", __FILE__, __LINE__, #cond);     \
        }                                                               \
    } while (0)

struct AddRequest {
    int16_t a;
    int16_t b;
};

struct AddResponse {
    int32_t sum;
};

constexpr uint32_t kRequestId = 0x300;
constexpr uint32_t kResponseId = 0x301;

using Client = can_rpc::CanRpcClient<AddRequest, AddResponse>;
using Server = can_rpc::CanRpcServer<AddRequest, AddResponse>;

struct Fixture {
    host::MockBus bus;
    host::MockCan client_can{bus};
    host::MockCan server_can{bus};
    host::EventQueue client_q;
    host::EventQueue server_q;
    Client client;
    Server server;

    int handler_calls = 0;
    int errors = 0;
    Error last_error = Error::Timeout;
    std::vector<int32_t> sums;

    explicit Fixture(uint8_t max_retries = 2, uint8_t initial_seq = 0)
        : client(client_can, client_q, ClientConfig{kRequestId, kResponseId, 100ms, max_retries, initial_seq}),
          server(server_can, server_q, ServerConfig{kRequestId, kResponseId}) {
        server.set_request_handler([this](const AddRequest& req) {
            ++handler_calls;
            return AddResponse{static_cast<int32_t>(req.a) + req.b};
        });
        client.set_response_callback([this](const AddResponse& res) { sums.push_back(res.sum); });
        client.set_error_callback([this](Error e) {
            ++errors;
            last_error = e;
        });
    }

    void pump() {
        for (int i = 0; i < 4; ++i) {
            client_q.run_ready();
            server_q.run_ready();
        }
    }

    void step(int ms) {
        for (int i = 0; i < ms; ++i) {
            client_q.advance(1);
            server_q.advance(1);
            pump();
        }
    }
};

CanFrame make_frame(uint32_t id, uint8_t len, uint8_t seq) {
    CanFrame f;
    f.id = id;
    f.len = len;
    f.data[0] = seq;
    return f;
}

void test_normal_call() {
    Fixture fx;
    CHECK(fx.client.call_async(AddRequest{2, 3}) == Status::Ok);
    fx.pump();
    CHECK(fx.sums.size() == 1);
    CHECK(fx.sums.size() == 1 && fx.sums[0] == 5);
    CHECK(fx.handler_calls == 1);
    CHECK(fx.errors == 0);
    CHECK(fx.client_q.pending() == 0);  // タイムアウトがキャンセルされている
}

void test_negative_values() {
    Fixture fx;
    CHECK(fx.client.call_async(AddRequest{-30000, -30000}) == Status::Ok);
    fx.pump();
    CHECK(fx.sums.size() == 1 && fx.sums[0] == -60000);
}

void test_busy_and_sequence() {
    Fixture fx;
    CHECK(fx.client.call_async(AddRequest{1, 1}) == Status::Ok);
    CHECK(fx.client.call_async(AddRequest{9, 9}) == Status::Busy);
    fx.pump();
    CHECK(fx.client.call_async(AddRequest{2, 2}) == Status::Ok);
    fx.pump();
    CHECK(fx.sums.size() == 2);
    CHECK(fx.client_can.sent.size() == 2);
    CHECK(fx.client_can.sent[0].data[0] == 0);
    CHECK(fx.client_can.sent[1].data[0] == 1);
    CHECK(fx.handler_calls == 2);
}

void test_request_lost_then_retry() {
    Fixture fx;
    int dropped = 0;
    fx.bus.drop_if = [&](const CanFrame& f) { return f.id == kRequestId && dropped++ < 1; };

    CHECK(fx.client.call_async(AddRequest{4, 5}) == Status::Ok);
    fx.step(50);
    CHECK(fx.handler_calls == 0);
    fx.step(60);  // 100 ms 経過で再送
    CHECK(fx.client_can.sent.size() == 2);
    CHECK(fx.client_can.sent.size() == 2 && fx.client_can.sent[0].data[0] == fx.client_can.sent[1].data[0]);
    CHECK(fx.handler_calls == 1);
    CHECK(fx.sums.size() == 1 && fx.sums[0] == 9);
    CHECK(fx.errors == 0);
}

void test_response_lost_is_deduplicated() {
    Fixture fx;
    int dropped = 0;
    fx.bus.drop_if = [&](const CanFrame& f) { return f.id == kResponseId && dropped++ < 1; };

    CHECK(fx.client.call_async(AddRequest{7, 8}) == Status::Ok);
    fx.step(150);
    CHECK(fx.handler_calls == 1);  // 再送要求では handler を呼ばない
    CHECK(fx.server_can.sent.size() == 2);
    CHECK(fx.client_can.sent.size() == 2);
    CHECK(fx.sums.size() == 1 && fx.sums[0] == 15);
    CHECK(fx.errors == 0);
}

void test_timeout_after_retries() {
    Fixture fx(2);
    fx.bus.drop_if = [](const CanFrame& f) { return f.id == kRequestId; };

    CHECK(fx.client.call_async(AddRequest{1, 2}) == Status::Ok);
    fx.step(50);
    CHECK(fx.errors == 0);
    fx.step(400);
    CHECK(fx.errors == 1);
    CHECK(fx.last_error == Error::Timeout);
    CHECK(fx.client_can.sent.size() == 3);  // 初回 + 再送 2 回
    CHECK(fx.client_q.pending() == 0);

    fx.bus.drop_if = nullptr;
    CHECK(fx.client.call_async(AddRequest{1, 2}) == Status::Ok);  // busy が解除されている
    fx.pump();
    CHECK(fx.sums.size() == 1 && fx.sums[0] == 3);
}

void test_zero_retries() {
    Fixture fx(0);
    fx.bus.drop_if = [](const CanFrame& f) { return f.id == kRequestId; };
    CHECK(fx.client.call_async(AddRequest{1, 2}) == Status::Ok);
    fx.step(150);
    CHECK(fx.errors == 1);
    CHECK(fx.last_error == Error::Timeout);
    CHECK(fx.client_can.sent.size() == 1);
}

void test_send_failed_is_synchronous() {
    Fixture fx;
    fx.client_can.fail_next_writes = 1;
    CHECK(fx.client.call_async(AddRequest{1, 1}) == Status::SendFailed);
    CHECK(fx.client_q.pending() == 0);  // タイムアウトは設定されない
    CHECK(fx.client.call_async(AddRequest{1, 1}) == Status::Ok);  // busy にならない
    fx.pump();
    CHECK(fx.sums.size() == 1);
    // 失敗した call は seq を消費しない
    CHECK(fx.client_can.sent.size() == 2);
    CHECK(fx.client_can.sent[0].data[0] == 0);
    CHECK(fx.client_can.sent[1].data[0] == 0);
}

void test_resend_failure_reports_error() {
    Fixture fx;
    fx.bus.drop_if = [](const CanFrame& f) { return f.id == kRequestId; };
    CHECK(fx.client.call_async(AddRequest{1, 1}) == Status::Ok);
    fx.client_can.fail_next_writes = 1;
    fx.step(110);
    CHECK(fx.errors == 1);
    CHECK(fx.last_error == Error::SendFailed);
    CHECK(fx.client.call_async(AddRequest{1, 1}) == Status::Ok);
}

void test_client_ignores_unrelated_frames() {
    Fixture fx;
    CHECK(fx.client.call_async(AddRequest{10, 20}) == Status::Ok);
    const size_t pending_before = fx.client_q.pending();

    fx.client_can.receive(make_frame(0x555, 5, 0));            // 無関係な ID
    CHECK(fx.client_q.pending() == pending_before);            // ISR 側で破棄されキューに入らない
    fx.client_can.receive(make_frame(kResponseId, 5, 9));      // seq 不一致
    fx.client_can.receive(make_frame(kResponseId, 2, 0));      // 長さ不一致
    fx.client_q.run_ready();
    CHECK(fx.sums.empty());

    fx.pump();  // 正規の応答を処理
    CHECK(fx.sums.size() == 1 && fx.sums[0] == 30);
}

void test_response_without_call_is_ignored() {
    Fixture fx;
    fx.client_can.receive(make_frame(kResponseId, 5, 0));
    fx.pump();
    CHECK(fx.sums.empty());
    CHECK(fx.errors == 0);
}

void test_call_from_response_callback() {
    Fixture fx;
    Client* client = &fx.client;
    fx.client.set_response_callback([&fx, client](const AddResponse& res) {
        fx.sums.push_back(res.sum);
        if (fx.sums.size() < 3) {
            CHECK(client->call_async(AddRequest{1, static_cast<int16_t>(fx.sums.size())}) == Status::Ok);
        }
    });
    CHECK(fx.client.call_async(AddRequest{1, 0}) == Status::Ok);
    fx.pump();
    CHECK(fx.sums.size() == 3);
    CHECK(fx.handler_calls == 3);
}

void test_sequence_wraps() {
    Fixture fx(2, 254);
    for (int i = 0; i < 3; ++i) {
        CHECK(fx.client.call_async(AddRequest{1, 1}) == Status::Ok);
        fx.pump();
    }
    CHECK(fx.client_can.sent.size() == 3);
    CHECK(fx.client_can.sent[0].data[0] == 254);
    CHECK(fx.client_can.sent[1].data[0] == 255);
    CHECK(fx.client_can.sent[2].data[0] == 0);
    CHECK(fx.handler_calls == 3);
}

void test_server_ignores_malformed_request() {
    Fixture fx;
    fx.server_can.receive(make_frame(kRequestId, 2, 0));  // 長さ不一致
    fx.server_can.receive(make_frame(0x555, 5, 0));       // 無関係な ID
    CHECK(fx.server_q.pending() == 1);  // ISR は ID のみ判定するため、長さ不一致は投入され無関係な ID は投入されない
    fx.pump();
    CHECK(fx.handler_calls == 0);
    CHECK(fx.server_can.sent.empty());
}

void test_server_without_handler_stays_silent() {
    host::MockBus bus;
    host::MockCan can{bus};
    host::EventQueue queue;
    Server server(can, queue, ServerConfig{kRequestId, kResponseId});
    can.receive(make_frame(kRequestId, 5, 0));
    queue.run_ready();
    CHECK(can.sent.empty());
}

void test_destructor_detaches() {
    host::MockBus bus;
    host::MockCan can{bus};
    host::EventQueue queue;
    {
        Client client(can, queue, ClientConfig{kRequestId, kResponseId, 100ms, 1});
        CHECK(client.call_async(AddRequest{1, 1}) == Status::Ok);
    }
    CHECK(queue.pending() == 0);  // タイムアウトが取り消されている
    can.receive(make_frame(kResponseId, 5, 0));
    CHECK(queue.pending() == 0);  // ハンドラが解除されている
}

void test_future_success() {
    Fixture fx;
    auto future = fx.client.call(AddRequest{2, 3});
    CHECK(!future.is_ready());  // 送信は dispatch コンテキストで行われる
    fx.pump();
    CHECK(future.is_ready());
    const auto result = await(future);
    CHECK(result.ok());
    CHECK(result.value().sum == 5);
    CHECK(fx.handler_calls == 1);
    CHECK(fx.client_q.pending() == 0);

    const auto again = await(future);  // 再度 await しても同じ結果
    CHECK(again.ok() && again.value().sum == 5);
}

void test_future_sequential_calls() {
    Fixture fx;
    for (int i = 0; i < 3; ++i) {
        auto future = fx.client.call(AddRequest{static_cast<int16_t>(i), 10});
        fx.pump();
        const auto result = await(future);
        CHECK(result && result.value().sum == 10 + i);
    }
    CHECK(fx.client_can.sent.size() == 3);
}

void test_future_timeout() {
    Fixture fx(1);
    fx.bus.drop_if = [](const CanFrame& f) { return f.id == kRequestId; };
    auto future = fx.client.call(AddRequest{1, 2});
    fx.step(50);
    CHECK(!future.is_ready());
    fx.step(300);
    CHECK(future.is_ready());
    const auto result = await(future);
    CHECK(!result.ok());
    CHECK(result.error() == Error::Timeout);
    CHECK(fx.client_can.sent.size() == 2);  // 初回 + 再送 1 回
}

void test_future_busy() {
    Fixture fx;
    auto first = fx.client.call(AddRequest{1, 1});
    auto second = fx.client.call(AddRequest{9, 9});
    fx.client_q.run_ready();  // first の送信 → second は busy
    CHECK(second.is_ready());
    CHECK(await(second).error() == Error::Busy);
    CHECK(!first.is_ready());
    fx.pump();
    CHECK(await(first).ok());
    CHECK(fx.handler_calls == 1);
}

void test_future_send_failed() {
    Fixture fx;
    fx.client_can.fail_next_writes = 1;
    auto future = fx.client.call(AddRequest{1, 1});
    fx.pump();
    CHECK(await(future).error() == Error::SendFailed);
    auto retry = fx.client.call(AddRequest{1, 1});  // busy が残っていない
    fx.pump();
    CHECK(await(retry).ok());
}

void test_future_resend_failure() {
    Fixture fx;
    fx.bus.drop_if = [](const CanFrame& f) { return f.id == kRequestId; };
    auto future = fx.client.call(AddRequest{1, 1});
    fx.client_q.run_ready();
    fx.client_can.fail_next_writes = 1;
    fx.step(110);
    CHECK(await(future).error() == Error::SendFailed);
}

void test_future_cancelled_on_destroy() {
    host::MockBus bus;
    host::MockCan can{bus};
    host::EventQueue queue;
    auto make = [&]() {
        Client client(can, queue, ClientConfig{kRequestId, kResponseId, 100ms, 1});
        auto future = client.call(AddRequest{1, 1});
        queue.run_ready();  // 送信済みで応答待ちの状態
        return future;
    };
    auto future = make();  // client は破棄済み
    CHECK(future.is_ready());
    CHECK(await(future).error() == Error::Cancelled);
}

void test_future_and_callback_both_notified() {
    Fixture fx;
    auto future = fx.client.call(AddRequest{4, 4});
    fx.pump();
    CHECK(await(future).value().sum == 8);
    CHECK(fx.sums.size() == 1 && fx.sums[0] == 8);
}

}  // namespace

int main() {
    test_normal_call();
    test_negative_values();
    test_busy_and_sequence();
    test_request_lost_then_retry();
    test_response_lost_is_deduplicated();
    test_timeout_after_retries();
    test_zero_retries();
    test_send_failed_is_synchronous();
    test_resend_failure_reports_error();
    test_client_ignores_unrelated_frames();
    test_response_without_call_is_ignored();
    test_call_from_response_callback();
    test_sequence_wraps();
    test_server_ignores_malformed_request();
    test_server_without_handler_stays_silent();
    test_destructor_detaches();
    test_future_success();
    test_future_sequential_calls();
    test_future_timeout();
    test_future_busy();
    test_future_send_failed();
    test_future_resend_failure();
    test_future_cancelled_on_destroy();
    test_future_and_callback_both_notified();

    printf("[TEST] checks=%d failures=%d\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

# CanRpc

Mbed OS 上の Classic CAN で、型付き payload の RPC を行うヘッダオンリーの C++ ライブラリ。

要件: [docs/requirements.md](docs/requirements.md)

## 使い方

```cpp
#include "can_rpc/can_rpc.hpp"
#include "can_rpc/mbed_can_interface.hpp"

struct AddRequest  { int16_t a; int16_t b; };
struct AddResponse { int32_t sum; };

Step::CAN can(PB_8, PB_9, 1000000);
can_rpc::MbedCanInterface<Step::CAN> can_interface(can);
events::EventQueue queue(4 * 1024);

// client
can_rpc::CanRpcClient<AddRequest, AddResponse> client(
    can_interface, queue, can_rpc::ClientConfig{0x300, 0x301, 100ms, 3});

// server
can_rpc::CanRpcServer<AddRequest, AddResponse> server(
    can_interface, queue, can_rpc::ServerConfig{0x300, 0x301});
server.set_request_handler([](const AddRequest& r) { return AddResponse{r.a + r.b}; });

// dispatch は専用スレッドで回す
Thread dispatch_thread;
dispatch_thread.start([]() { queue.dispatch_forever(); });

// 別スレッド (main など) から await 風に呼ぶ
const can_rpc::Result<AddResponse> result = await(client.call(AddRequest{1, 2}));
if (result) { /* result.value().sum */ } else { /* result.error() */ }
```

`call()` は `Future` を返し、`await()` が完了までブロックする。dispatch スレッド上で `await()` するとデッドロックするため、必ず別スレッドから呼ぶ。
dispatch コンテキスト内で完結させたい場合は callback 方式の `call_async()` + `set_response_callback()` / `set_error_callback()` を使う。

## ディレクトリ

| パス | 内容 |
|---|---|
| `include/can_rpc/` | ライブラリ本体 |
| `examples/add_client`, `examples/add_server` | Add(a, b) のサンプル (nucleo_f303k8) |
| `host_test/` | ホスト (PC) 上のテスト。CMake + MSVC / GCC |

## ホストテスト

```
cmake -S host_test -B build/host
cmake --build build/host --config Debug
build/host/Debug/can_rpc_host_test
```

## サンプルのビルド

`Step::CAN` を使用するため、robostep-libs-cpp (private) を `lib_deps` で `.pio/libdeps` に取得する。GitHub に SSH 鍵で接続できること。

```
cd examples/add_client && pio run
cd examples/add_server && pio run
```

F303K8 の CAN は D10=PA_11 (RD) / D2=PA_12 (TD)。

2 枚の NUCLEO-F303K8 を CAN トランシーバ経由で接続し、それぞれに client / server を書き込む。

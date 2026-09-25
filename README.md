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
client.set_response_callback([](const AddResponse& r) { /* ... */ });
client.set_error_callback([](can_rpc::Error e) { /* ... */ });
client.call(AddRequest{1, 2});  // EventQueue の dispatch コンテキストから呼ぶ

// server
can_rpc::CanRpcServer<AddRequest, AddResponse> server(
    can_interface, queue, can_rpc::ServerConfig{0x300, 0x301});
server.set_request_handler([](const AddRequest& r) { return AddResponse{r.a + r.b}; });

queue.dispatch_forever();
```

## ディレクトリ

| パス | 内容 |
|---|---|
| `include/can_rpc/` | ライブラリ本体 |
| `examples/add_client`, `examples/add_server` | Add(a, b) のサンプル (nucleo_f446re) |
| `host_test/` | ホスト (PC) 上のテスト。CMake + MSVC / GCC |

## ホストテスト

```
cmake -S host_test -B build/host
cmake --build build/host --config Debug
build/host/Debug/can_rpc_host_test
```

## サンプルのビルド

`Step::CAN` を使用するため、環境変数 `ROBOSTEP_LIBS_CPP` に robostep-libs-cpp のパスを設定する。

```
set ROBOSTEP_LIBS_CPP=C:/path/to/nhk-r1/libs/robostep-libs-cpp
cd examples/add_client && pio run
cd examples/add_server && pio run
```

2 枚の NUCLEO-F446RE を CAN トランシーバ経由で接続し、それぞれに client / server を書き込む。

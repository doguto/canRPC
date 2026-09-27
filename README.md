# CanRpc

Mbed OS 上の Classic CAN で、型付き payload の RPC を行うヘッダオンリーの C++ ライブラリ。

要件: [docs/requirements.md](docs/requirements.md)

## 使い方

```cpp
#include "can_rpc/can_rpc.hpp"

struct AddRequest  { int16_t a; int16_t b; };
struct AddResponse { int32_t sum; };

// CAN・EventQueue・dispatch スレッドをまとめて用意する
can_rpc::Node node(PB_8, PB_9, 1000000);  // rd, td, bitrate

// client (timeout / retries は省略時 100ms / 3 回)
auto client = node.client<AddRequest, AddResponse>(0x300, 0x301);

// server (handler は Node の dispatch スレッドで実行される)
auto server = node.server<AddRequest, AddResponse>(0x300, 0x301, [](const AddRequest& r) {
    return AddResponse{r.a + r.b};
});

int main() {
    // main など dispatch スレッド以外から await 風に呼ぶ
    const can_rpc::Result<AddResponse> result = await(client.call(AddRequest{1, 2}));
    if (result) { /* result.value().sum */ } else { /* result.error() */ }
}
```

- `Node` は生成時に dispatch スレッドを起動する。EventQueue とスレッドの stack は `Node` 内に静的に確保されるため、グローバルに置けば RAM 使用量はリンク時に確定する
- サイズを変える場合は `can_rpc::BasicNode<QueueSize, StackSize, MaxHandlers>` を使う (既定 1024 / 1536 / 4)
- `node.client()` / `node.server()` の戻り値はそのまま変数に受ける (C++17 が必要)
- `call()` の `await()` は dispatch スレッド上 (callback / handler 内) で呼ぶとデッドロックする。その場合は callback 方式の `call_async()` + `set_response_callback()` / `set_error_callback()` を使う
- EventQueue やスレッドを自前で管理したい場合は、`can_rpc::MbedCanInterface` と `CanRpcClient` / `CanRpcServer` を直接組み合わせて使える

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

```
cd examples/add_client && pio run
cd examples/add_server && pio run
```

F303K8 の CAN は D10=PA_11 (RD) / D2=PA_12 (TD)。

2 枚の NUCLEO-F303K8 を CAN トランシーバ経由で接続し、それぞれに client / server を書き込む。

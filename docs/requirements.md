# CanRpc 実装要件 (v1)

## 目的

Mbed OS 上の Classic CAN で、型付き payload の RPC (Request / Response) を行う C++ テンプレートライブラリ。

## 構成

- `CanRpcClient<RequestPayload, ResponsePayload>`
- `CanRpcServer<RequestPayload, ResponsePayload>`
- `CanInterface` : CAN 下位層の抽象 (write / attach / detach)
  - `MbedCanInterface<CanT>` : `Step::CAN` (robostep-libs-cpp) 互換の CAN クラス向けアダプタ
  - `host_test/mock_can.hpp` : ホストテスト用モック
- 言語: C++14 以上 (ヘッダオンリー)
- 対象: Mbed OS 6 / PlatformIO

## 制約

- Classic CAN 専用。`CANMessage::data` は最大 8 byte
- `static_assert(sizeof(RequestPayload) <= 7 && sizeof(ResponsePayload) <= 7)`
  - 1 byte はシーケンス番号に使用する
  - 分割送信は v1 のスコープ外
- payload は trivially copyable な POD 構造体を `memcpy` でシリアライズする
  - 通信する両ノードで同一のエンディアン・構造体レイアウトであること (ARM 同士を想定)
- 同時に実行できる call は 1 つ

## フレーム形式

| 項目 | 内容 |
|---|---|
| CAN ID | request 用 / response 用の 2 本。コンストラクタで指定 |
| `data[0]` | シーケンス番号 (0〜255 を循環。初期値は `ClientConfig::initial_seq`、既定 0) |
| `data[1..]` | POD payload |
| `len` | `1 + sizeof(Payload)` |

## Client API

```cpp
CanRpcClient(CanInterface&, EventQueue&, ClientConfig{request_id, response_id, timeout, max_retries, initial_seq});
Future<ResponsePayload> call(const RequestPayload&);
Result<ResponsePayload> await(Future<ResponsePayload>);  // call().await() でも可

Status call_async(const RequestPayload&);
void set_response_callback(Callback<void(const ResponsePayload&)>);
void set_error_callback(Callback<void(Error)>);
```

- `call()` は任意のスレッドから呼べる。送信は `EventQueue` に投入され、dispatch コンテキストで行われる
- `await()` は完了までブロックし `Result` を返す。完了後は何度呼んでも同じ結果を返す
- `Result`: `ok()` / `value()` / `error()`。`Error`: `Timeout` / `SendFailed` / `Busy` / `QueueFull` / `Cancelled`
  - `Busy`: 実行中の call あり。`QueueFull`: `EventQueue` に投入できなかった。`Cancelled`: 完了前に client が破棄された
- `call_async()` は dispatch コンテキスト用。送信のみを行い、結果は callback で通知する
  - 同期戻り値: `Ok` / `Busy` (実行中の call あり) / `SendFailed`
  - `call()` の結果は Future と callback の両方に通知される

## Server API

```cpp
CanRpcServer(CanInterface&, EventQueue&, ServerConfig{request_id, response_id});
void set_request_handler(Callback<ResponsePayload(const RequestPayload&)>);
```

## 動作仕様

- タイムアウトは `EventQueue::call_in` で駆動する
- タイムアウト時は同一シーケンス番号で再送し、`max_retries` を超えたら error callback (`Timeout`) を呼ぶ
- 再送時の送信が失敗した場合は error callback (`SendFailed`) を呼ぶ
- callback の呼び出し前に実行中状態を解除する (callback 内から `call()` を再度呼べる)
- client は、シーケンス番号・長さが一致しない response を破棄する
- server は最後に処理したシーケンス番号と応答をキャッシュする。同一番号のリクエストを受信した場合は handler を呼ばず、保存した応答を再送する (at-least-once 対策)
- 受信ハンドラ (ISR コンテキスト) は CAN ID の一致確認のみ行い、フレームを値渡しで `EventQueue` に投入する。処理と callback は `EventQueue` 側で実行する
- `EventQueue` はコンストラクタで外部から注入する。スレッドはライブラリ内で生成しない
- `CanInterface::attach()` は複数登録可能とし、同一の CAN に client / server / 他のリスナーを同居できる

## スレッド規約

- `call_async()` と全ての callback は `EventQueue` の dispatch コンテキストで実行する
- `call()` は dispatch スレッド以外から呼ぶ。dispatch スレッド上で `await()` するとデッドロックする
- インスタンスの破棄は `EventQueue` が停止している (または保留イベントが無い) 状態で行う

## 既知の制限

- クライアント再起動後に `initial_seq` が前回の最終 seq と一致すると、server が重複と判断してキャッシュ済みの応答を返す。開発中の再書き込み等で発生し得るため、必要に応じて `initial_seq` を起動ごとに変える
- 送信失敗 (`write()` が false) はリトライせずエラーとする
- ISR からの `EventQueue` 投入に失敗した (キュー満杯) フレームは失われる。client はタイムアウト再送で回復する

## スコープ外 (v2 以降)

- 8 byte を超える payload の分割送信
- CAN FD 対応
- 複数の call の並行実行
- node ID からの CAN ID 自動生成
- 拡張 ID

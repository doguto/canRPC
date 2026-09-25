#pragma once

#include <stdint.h>

#include <chrono>

namespace can_rpc {

// call_async() の同期戻り値
enum class Status : uint8_t {
    Ok,
    Busy,        // 実行中の call が存在する
    SendFailed,  // CAN 送信が受理されなかった
};

// error callback / Result に通知するエラー種別
enum class Error : uint8_t {
    Timeout,     // リトライ上限までに response が得られなかった
    SendFailed,  // CAN 送信が受理されなかった
    Busy,        // 実行中の call が存在する (call() のみ)
    QueueFull,   // EventQueue に投入できなかった (call() のみ)
    Cancelled,   // 完了前に client が破棄された (call() のみ)
};

// call() の結果。成功時は value()、失敗時は error() が有効。
template <typename T>
class Result {
public:
    Result() = default;

    static Result success(const T& value) {
        Result r;
        r.ok_ = true;
        r.value_ = value;
        return r;
    }

    static Result failure(Error error) {
        Result r;
        r.error_ = error;
        return r;
    }

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }
    const T& value() const { return value_; }  // ok() == true の場合のみ有効
    Error error() const { return error_; }     // ok() == false の場合のみ有効

private:
    bool ok_ = false;
    T value_{};
    Error error_ = Error::Cancelled;
};

struct ClientConfig {
    uint32_t request_id;
    uint32_t response_id;
    std::chrono::milliseconds timeout;  // 1 回の送信あたりの応答待ち時間
    uint8_t max_retries;                // 初回送信を除く再送回数
    uint8_t initial_seq = 0;            // 最初の call に使用するシーケンス番号
};

struct ServerConfig {
    uint32_t request_id;
    uint32_t response_id;
};

}  // namespace can_rpc

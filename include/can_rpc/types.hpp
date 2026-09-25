#pragma once

#include <stdint.h>

#include <chrono>

namespace can_rpc {

// call() の同期戻り値
enum class Status : uint8_t {
    Ok,
    Busy,        // 実行中の call が存在する
    SendFailed,  // CAN 送信が受理されなかった
};

// error callback に通知するエラー種別
enum class Error : uint8_t {
    Timeout,     // リトライ上限までに response が得られなかった
    SendFailed,  // 再送時に CAN 送信が受理されなかった
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

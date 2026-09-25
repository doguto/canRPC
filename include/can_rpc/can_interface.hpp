#pragma once

#include <stddef.h>
#include <stdint.h>

#include "platform.hpp"

namespace can_rpc {

// Classic CAN 1 フレーム (標準 ID のみ)
struct CanFrame {
    uint32_t id = 0;
    uint8_t len = 0;
    uint8_t data[8] = {};
};

// CAN 下位層の抽象。実装は MbedCanInterface / テスト用の MockCanInterface。
class CanInterface {
public:
    using RxHandler = std::function<void(const CanFrame&)>;

    static constexpr size_t kInvalidHandle = static_cast<size_t>(-1);

    virtual ~CanInterface() = default;

    // 送信。受理された場合 true。
    virtual bool write(const CanFrame& frame) = 0;

    // 受信ハンドラを登録する。複数登録可能。
    // ハンドラは ISR コンテキストで呼ばれる場合があるため、ブロッキング処理を行わないこと。
    // 戻り値は detach() に渡すハンドル。失敗時は kInvalidHandle。
    virtual size_t attach(RxHandler handler) = 0;

    virtual void detach(size_t handle) = 0;
};

}  // namespace can_rpc

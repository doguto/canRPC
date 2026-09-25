#pragma once

#include <string.h>

#include "can_interface.hpp"

namespace can_rpc {

// Step::CAN (robostep-libs-cpp) と同じ API を持つ CAN クラス向けのアダプタ。
//
// CanT の要件:
//   int    write(const mbed::CANMessage&)
//   size_t attach(std::function<void(mbed::CANMessage)>)  // 複数登録可能。戻り値はハンドル
//   void   detach(size_t handle)
//
// mbed::CAN / mbed::RawCAN 単体は attach() が 1 スロットのため、そのままでは使用できない。
template <typename CanT>
class MbedCanInterface : public CanInterface {
public:
    explicit MbedCanInterface(CanT& can) : can_(can) {}

    bool write(const CanFrame& frame) override {
        mbed::CANMessage msg(frame.id, frame.data, frame.len);
        return can_.write(msg) != 0;
    }

    size_t attach(RxHandler handler) override {
        return can_.attach([handler](mbed::CANMessage msg) {
            // 標準 ID のデータフレームのみ対象とする
            if (msg.format != CANStandard || msg.type != CANData || msg.len > 8) {
                return;
            }
            CanFrame frame;
            frame.id = msg.id;
            frame.len = msg.len;
            memcpy(frame.data, msg.data, msg.len);
            handler(frame);
        });
    }

    void detach(size_t handle) override { can_.detach(handle); }

private:
    CanT& can_;
};

}  // namespace can_rpc

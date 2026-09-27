#pragma once

#include <string.h>

#include "can_interface.hpp"
#include "drivers/RawCAN.h"
#include "platform/CriticalSectionLock.h"
#include "rtos/Mutex.h"

namespace can_rpc {

// mbed の CAN を直接使用する CanInterface の実装。
//
//   can_rpc::MbedCanInterface can(PA_11, PA_12, 1000000);  // rd, td, bitrate
//
// mbed::CAN の attach() は 1 スロットのため、受信割り込みを 1 つだけ登録し、
// 本クラス内で複数の受信ハンドラ (最大 MaxHandlers 個) へ配信する。
//
// 受信割り込み内で read() する必要があるため、内部で mbed::RawCAN (mutex 無し) を保持する。
// mbed::CAN の read() は mutex を取るため ISR から呼べない。
// RawCAN はスレッドセーフではないため、write() は本クラスの mutex で直列化する。
template <size_t MaxHandlers = 4>
class BasicMbedCanInterface : public CanInterface {
public:
    BasicMbedCanInterface(PinName rd, PinName td, int hz) : can_(rd, td, hz) {
        can_.attach(mbed::callback(this, &BasicMbedCanInterface::on_rx_irq), mbed::CAN::RxIrq);
    }

    ~BasicMbedCanInterface() override { can_.attach(nullptr, mbed::CAN::RxIrq); }

    BasicMbedCanInterface(const BasicMbedCanInterface&) = delete;
    BasicMbedCanInterface& operator=(const BasicMbedCanInterface&) = delete;

    // フィルタやモード設定など、RawCAN を直接操作したい場合に使う。
    // read() / attach() は本クラスが使用するため呼ばないこと。
    mbed::RawCAN& raw() { return can_; }

    // スレッドコンテキストから呼ぶこと (ISR 不可)
    bool write(const CanFrame& frame) override {
        const mbed::CANMessage msg(frame.id, frame.data, frame.len);
        write_mutex_.lock();
        const int ret = can_.write(msg);
        write_mutex_.unlock();
        return ret != 0;
    }

    size_t attach(RxHandler handler) override {
        mbed::CriticalSectionLock lock;
        for (size_t i = 0; i < MaxHandlers; i++) {
            if (!handlers_[i]) {
                handlers_[i] = std::move(handler);
                return i;
            }
        }
        return kInvalidHandle;
    }

    void detach(size_t handle) override {
        if (handle >= MaxHandlers) {
            return;
        }
        RxHandler removed;
        {
            mbed::CriticalSectionLock lock;
            removed.swap(handlers_[handle]);
        }
        // removed の破棄はクリティカルセクション外で行う
    }

private:
    // ISR コンテキスト。受信 FIFO が空になるまで読み出して配信する。
    void on_rx_irq() {
        mbed::CANMessage msg;
        while (can_.read(msg)) {
            // 標準 ID のデータフレームのみ対象とする
            if (msg.format != CANStandard || msg.type != CANData || msg.len > 8) {
                continue;
            }
            CanFrame frame;
            frame.id = msg.id;
            frame.len = msg.len;
            memcpy(frame.data, msg.data, msg.len);
            for (size_t i = 0; i < MaxHandlers; i++) {
                if (handlers_[i]) {
                    handlers_[i](frame);
                }
            }
        }
    }

    mbed::RawCAN can_;
    rtos::Mutex write_mutex_;
    RxHandler handlers_[MaxHandlers];
};

using MbedCanInterface = BasicMbedCanInterface<>;

}  // namespace can_rpc

#pragma once

// プラットフォーム依存の型をここに集約する。
// CAN_RPC_HOST_TEST 定義時はホストテスト用の代替実装を使用する。

#include <functional>

#if defined(CAN_RPC_HOST_TEST)
#include "host_event_queue.hpp"
#include "host_semaphore.hpp"
#else
#include "mbed.h"
#include "events/EventQueue.h"
#include "rtos/Semaphore.h"
#endif

namespace can_rpc {

template <typename Signature>
using Callback = std::function<Signature>;

#if defined(CAN_RPC_HOST_TEST)
using EventQueue = host::EventQueue;
using Semaphore = host::Semaphore;
#else
using EventQueue = events::EventQueue;
using Semaphore = rtos::Semaphore;
#endif

}  // namespace can_rpc

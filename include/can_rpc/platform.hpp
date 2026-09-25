#pragma once

// プラットフォーム依存の型をここに集約する。
// CAN_RPC_HOST_TEST 定義時はホストテスト用の代替実装を使用する。

#include <functional>

#if defined(CAN_RPC_HOST_TEST)
#include "host_event_queue.hpp"
#else
#include "mbed.h"
#include "events/EventQueue.h"
#endif

namespace can_rpc {

template <typename Signature>
using Callback = std::function<Signature>;

#if defined(CAN_RPC_HOST_TEST)
using EventQueue = host::EventQueue;
#else
using EventQueue = events::EventQueue;
#endif

}  // namespace can_rpc

#pragma once

#include <stdint.h>

// Add(a, b) -> sum
struct AddRequest {
    int16_t a;
    int16_t b;
};

struct AddResponse {
    int32_t sum;
};

static_assert(sizeof(AddRequest) <= 7, "AddRequest must fit in 7 bytes");
static_assert(sizeof(AddResponse) <= 7, "AddResponse must fit in 7 bytes");

// 本来は can_id.hpp 等に集約する ID。サンプルのためここに定義する。
constexpr uint32_t ADD_REQUEST_ID = 0x300;
constexpr uint32_t ADD_RESPONSE_ID = 0x301;

#pragma once
#include <cstdint>

namespace ob {

using OrderId = uint64_t;
using Price   = int64_t;   // fixed-point: cents or ticks
using Qty     = uint64_t;

enum class Side : uint8_t { Buy, Sell };

struct Fill {
    OrderId maker_id;
    OrderId taker_id;
    Price   price;
    Qty     qty;
};

} // namespace ob

#pragma once
#include "types.hpp"

namespace ob {

// Intrusive doubly-linked list node embedded directly in the order.
// Avoids heap allocation per node and enables O(1) splice-out on cancel.
struct Order {
    OrderId id;
    Side    side;
    Price   price;
    Qty     qty;

    Order* prev{nullptr};
    Order* next{nullptr};

    Order(OrderId id_, Side side_, Price price_, Qty qty_)
        : id(id_), side(side_), price(price_), qty(qty_) {}
};

} // namespace ob

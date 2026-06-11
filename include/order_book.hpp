#pragma once
#include "price_level.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <functional>
#include <optional>
#include <stdexcept>

namespace ob {

// Price-time priority limit order book.
//
// Bids: std::map<Price, PriceLevel, std::greater<Price>>
//   → begin() is always the best (highest) bid — O(1) BBO access.
// Asks: std::map<Price, PriceLevel>
//   → begin() is always the best (lowest) ask — O(1) BBO access.
//
// order_index_: unordered_map<OrderId, Order*> for O(1) cancel lookup.
//
// Memory: orders are owned by the caller via add_limit_order which
// returns a raw pointer. The book does not own Order memory.
// Use an external pool for high-throughput scenarios.
class OrderBook {
public:
    using FillCallback = std::function<void(const Fill&)>;

    explicit OrderBook(FillCallback on_fill = nullptr)
        : on_fill_(std::move(on_fill)) {}

    // Non-copyable
    OrderBook(const OrderBook&)            = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    // --- Limit order ---
    // Allocates an Order via new (replace with pool for production).
    // Matches aggressively before resting, per price-time priority.
    // Returns pointer to the resting order (nullptr if fully filled).
    Order* add_limit_order(OrderId id, Side side, Price price, Qty qty);

    // --- Market order ---
    // Fills against best available price(s). Unmatched qty is dropped.
    void add_market_order(OrderId id, Side side, Qty qty);

    // --- Cancel ---
    // O(1): hash-map lookup → intrusive-list splice-out.
    // Returns false if order not found (already filled or bad id).
    bool cancel_order(OrderId id);

    // --- Best bid / offer ---
    std::optional<Price> best_bid() const noexcept;
    std::optional<Price> best_ask() const noexcept;

    // --- Introspection ---
    std::size_t bid_levels() const noexcept { return bids_.size(); }
    std::size_t ask_levels() const noexcept { return asks_.size(); }
    std::size_t order_count() const noexcept { return order_index_.size(); }

private:
    using BidMap = std::map<Price, PriceLevel, std::greater<Price>>;
    using AskMap = std::map<Price, PriceLevel>;

    BidMap bids_;
    AskMap asks_;
    std::unordered_map<OrderId, Order*> order_index_;
    FillCallback on_fill_;

    // Internal: match qty against the opposite side, returning unmatched qty.
    template <typename LevelMap, typename PricePred>
    Qty match_against(OrderId taker_id, Side taker_side, Qty qty,
                      LevelMap& levels, PricePred price_acceptable);

    void emit_fill(OrderId maker, OrderId taker, Price p, Qty q);

    // Remove a price level from the map if empty.
    template <typename LevelMap>
    void prune_if_empty(LevelMap& levels, typename LevelMap::iterator it);
};

} // namespace ob

#include "order_book.hpp"
#include <limits>
#include <cassert>

namespace ob {

// ── helpers ──────────────────────────────────────────────────────────────────

void OrderBook::emit_fill(OrderId maker, OrderId taker, Price p, Qty q) {
    if (on_fill_) on_fill_(Fill{maker, taker, p, q});
}

template <typename LevelMap>
void OrderBook::prune_if_empty(LevelMap& levels,
                                typename LevelMap::iterator it) {
    if (it->second.empty()) levels.erase(it);
}

// Match `qty` of a taker order against `levels` (the opposite side).
// `price_acceptable(level_price)` returns true while the level is crossable.
template <typename LevelMap, typename PricePred>
Qty OrderBook::match_against(OrderId taker_id, Side /*taker_side*/, Qty qty,
                              LevelMap& levels, PricePred price_acceptable) {
    while (qty > 0 && !levels.empty()) {
        auto it = levels.begin();
        if (!price_acceptable(it->first)) break;

        PriceLevel& level = it->second;
        while (qty > 0 && !level.empty()) {
            Order* maker = level.front();
            Qty    trade  = std::min(qty, maker->qty);

            emit_fill(maker->id, taker_id, maker->price, trade);

            maker->qty -= trade;
            qty        -= trade;
            level.deduct_qty(trade);  // keep level total in sync
            if (maker->qty == 0) {
                level.unlink(maker);
                order_index_.erase(maker->id);
                delete maker;
            }
        }
        prune_if_empty(levels, it);
    }
    return qty;
}

// ── public API ────────────────────────────────────────────────────────────────

Order* OrderBook::add_limit_order(OrderId id, Side side, Price price, Qty qty) {
    if (order_index_.count(id))
        throw std::invalid_argument("duplicate order id");

    // Match aggressively first.
    if (side == Side::Buy) {
        // Crosses if ask_price <= limit_price
        qty = match_against(id, side, qty, asks_,
                            [price](Price ask) { return ask <= price; });
    } else {
        // Crosses if bid_price >= limit_price
        qty = match_against(id, side, qty, bids_,
                            [price](Price bid) { return bid >= price; });
    }

    if (qty == 0) return nullptr;  // fully filled

    // Rest the unfilled portion.
    auto* o = new Order(id, side, price, qty);
    order_index_.emplace(id, o);
    if (side == Side::Buy)
        bids_[price].push_back(o);
    else
        asks_[price].push_back(o);

    return o;
}

void OrderBook::add_market_order(OrderId id, Side side, Qty qty) {
    if (side == Side::Buy) {
        match_against(id, side, qty, asks_,
                      [](Price) { return true; });  // any ask is acceptable
    } else {
        match_against(id, side, qty, bids_,
                      [](Price) { return true; });
    }
    // Unmatched market-order qty is silently dropped per spec.
}

bool OrderBook::cancel_order(OrderId id) {
    auto it = order_index_.find(id);
    if (it == order_index_.end()) return false;

    Order* o = it->second;
    if (o->side == Side::Buy) {
        auto lit = bids_.find(o->price);
        lit->second.remove(o);
        prune_if_empty(bids_, lit);
    } else {
        auto lit = asks_.find(o->price);
        lit->second.remove(o);
        prune_if_empty(asks_, lit);
    }
    order_index_.erase(it);
    delete o;
    return true;
}

std::optional<Price> OrderBook::best_bid() const noexcept {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const noexcept {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

} // namespace ob

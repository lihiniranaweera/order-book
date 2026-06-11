#pragma once
#include "order.hpp"

namespace ob {

// Intrusive FIFO queue of orders at one price level.
// head = oldest (first to match), tail = newest.
// All operations are O(1).
class PriceLevel {
public:
    PriceLevel() = default;

    // Non-copyable: owns no memory but the pointers are structural
    PriceLevel(const PriceLevel&)            = delete;
    PriceLevel& operator=(const PriceLevel&) = delete;
    PriceLevel(PriceLevel&&)                 = default;
    PriceLevel& operator=(PriceLevel&&)      = default;

    void push_back(Order* o) noexcept {
        o->prev = tail_;
        o->next = nullptr;
        if (tail_) tail_->next = o; else head_ = o;
        tail_ = o;
        total_qty_ += o->qty;
        ++count_;
    }

    // Full removal: splice out + deduct remaining qty from level total.
    // Use for cancel and for orders whose qty is still non-zero on removal.
    void remove(Order* o) noexcept {
        unlink(o);
        total_qty_ -= o->qty;
    }

    // Pointer-only splice — does NOT touch total_qty_.
    // Use during matching after deduct_qty() has already been called.
    void unlink(Order* o) noexcept {
        if (o->prev) o->prev->next = o->next; else head_ = o->next;
        if (o->next) o->next->prev = o->prev; else tail_ = o->prev;
        o->prev = o->next = nullptr;
        --count_;
    }

    // Deduct a traded quantity from the level total (called before unlink
    // during matching so total stays accurate across partial fills).
    void deduct_qty(Qty q) noexcept { total_qty_ -= q; }

    Order* front()       noexcept { return head_; }
    bool   empty() const noexcept { return head_ == nullptr; }
    Qty    total_qty() const noexcept { return total_qty_; }
    std::size_t count() const noexcept { return count_; }

private:
    Order*      head_{nullptr};
    Order*      tail_{nullptr};
    Qty         total_qty_{0};
    std::size_t count_{0};
};

} // namespace ob

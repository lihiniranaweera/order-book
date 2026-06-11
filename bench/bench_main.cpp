#include <benchmark/benchmark.h>
#include "order_book.hpp"
#include <random>
#include <vector>

// ── helpers ───────────────────────────────────────────────────────────────────

static void populate_book(ob::OrderBook& book,
                          std::vector<ob::OrderId>& ids_out,
                          int n_bids, int n_asks,
                          ob::Price mid = 10000) {
    ob::OrderId id = static_cast<ob::OrderId>(ids_out.size()) + 1;

    // Spread bids below mid, asks above — guarantees no crossing.
    for (int i = 0; i < n_bids; ++i) {
        book.add_limit_order(id, ob::Side::Buy,  mid - 1 - (i % 50), 100);
        ids_out.push_back(id++);
    }
    for (int i = 0; i < n_asks; ++i) {
        book.add_limit_order(id, ob::Side::Sell, mid + 1 + (i % 50), 100);
        ids_out.push_back(id++);
    }
}

// ── Benchmark: add limit orders (no crossing) ─────────────────────────────────
// Measures pure insertion throughput into the resting book.

static void BM_AddLimitOrder(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));
    ob::Price mid = 10000;

    for (auto _ : state) {
        state.PauseTiming();
        ob::OrderBook book;
        ob::OrderId   id = 1;
        state.ResumeTiming();

        for (int i = 0; i < n; ++i) {
            // Alternate buy/sell around mid so levels accumulate.
            ob::Side  side  = (i % 2 == 0) ? ob::Side::Buy : ob::Side::Sell;
            ob::Price price = (side == ob::Side::Buy)
                              ? mid - 1 - (i % 50)
                              : mid + 1 + (i % 50);
            benchmark::DoNotOptimize(
                book.add_limit_order(id++, side, price, 100));
        }
    }

    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_AddLimitOrder)->Arg(1000)->Arg(10000)->Arg(100000);

// ── Benchmark: cancel order (O(1) hash-map + intrusive splice) ────────────────
// Pre-populates a book, then measures cancel latency per order.

static void BM_CancelOrder(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        std::vector<ob::OrderId> ids;
        ids.reserve(n);
        ob::OrderBook book;
        populate_book(book, ids, n / 2, n / 2);
        // Shuffle so we don't always cancel in insertion order.
        std::mt19937 rng(42);
        std::shuffle(ids.begin(), ids.end(), rng);
        state.ResumeTiming();

        for (ob::OrderId oid : ids) {
            benchmark::DoNotOptimize(book.cancel_order(oid));
        }
    }

    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_CancelOrder)->Arg(1000)->Arg(10000)->Arg(100000);

// ── Benchmark: add order throughput with matching ─────────────────────────────
// Flood the book with crossing market orders to measure match throughput.

static void BM_MarketOrderMatch(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));
    ob::Price mid = 10000;

    for (auto _ : state) {
        state.PauseTiming();
        ob::OrderBook book;
        ob::OrderId   id = 1;
        // Pre-fill ask side with n resting orders.
        for (int i = 0; i < n; ++i)
            book.add_limit_order(id++, ob::Side::Sell, mid + 1 + (i % 50), 100);
        state.ResumeTiming();

        // Single large market buy sweeps through all levels.
        book.add_market_order(id++, ob::Side::Buy,
                              static_cast<ob::Qty>(n) * 100);
    }

    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_MarketOrderMatch)->Arg(1000)->Arg(10000)->Arg(50000);

BENCHMARK_MAIN();

# Limit Order Book & Matching Engine

A price-time priority limit order book and matching engine implemented in C++17, built to demonstrate the core data structures and algorithms that underpin real exchange infrastructure.

The engine supports limit orders, market orders, and cancellations. Limit orders match aggressively against the opposite side before resting in the book; market orders sweep all available liquidity at any price; cancellations are processed in O(1) regardless of book depth.

## Architecture

The book is structured around three cooperating data structures:

**Price levels** are maintained in two `std::map` instances — one for bids (sorted descending via `std::greater<Price>`), one for asks (sorted ascending). This keeps the best bid and best offer permanently at `begin()`, making BBO access O(1) without any auxiliary tracking.

**Within each price level**, orders are held in an intrusive doubly-linked list — a FIFO queue that enforces time priority among orders at the same price. Each `Order` struct carries its own `prev`/`next` pointers, eliminating the per-node heap allocation of `std::list` and enabling O(1) splice-out given only a raw pointer.

**An `std::unordered_map<OrderId, Order*>`** indexes every resting order by ID. On cancellation, a single hash lookup yields a direct pointer to the order; the intrusive list then removes it in O(1) with no search.

Prices are stored as signed 64-bit integers (fixed-point ticks) to avoid floating-point rounding error in comparison and arithmetic.

## Complexity

| Operation | Complexity |
|---|---|
| Add limit order (no match) | O(log N) — price level insert |
| Add limit order (matches) | O(K log N) — K fills across levels |
| Add market order | O(K log N) |
| Cancel order | O(1) hash lookup + O(1) splice + O(log N) level prune |
| Best bid / best ask | O(1) |

N = number of distinct price levels. In practice N is small (tens to hundreds), making the log N term negligible.

## Benchmarks

Measured on a 4-core 2.8 GHz machine, Release build (`-O3 -march=native`):

| Benchmark | 1,000 orders | 10,000 orders | 100,000 orders |
|---|---|---|---|
| Add limit order | 10.3M / sec | 1.8M / sec | 1.9M / sec |
| Cancel order | 10.8M / sec | 5.7M / sec | 3.5M / sec |
| Market order match | 20.4M / sec | 10.1M / sec | 9.2M / sec *(50k)* |

Throughput drops at larger scales as the working set exceeds L1/L2 cache.

## Build

Requires CMake 3.16+ and a C++17 compiler. Google Benchmark is fetched automatically if not installed.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/ob_bench
```

For a debug build with AddressSanitizer and UBSan:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

## Project Structure

```
include/
  types.hpp        # OrderId, Price, Qty, Side, Fill
  order.hpp        # Order struct with intrusive list pointers
  price_level.hpp  # Intrusive FIFO queue at one price
  order_book.hpp   # Book interface and internal structure
src/
  order_book.cpp   # Matching engine implementation
bench/
  bench_main.cpp   # Google Benchmark suite
CMakeLists.txt
```

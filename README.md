# Order Book & Matching Engine (C++20)

A limit order book for a single instrument with **price-time priority** matching,
the same core rule used by equity and futures exchanges.

## Status

| Stage | Scope | Status |
|---|---|---|
| 1 | Limit orders, price-time matching, cancel, tests | ✅ Done |
| 2 | Market orders, modify, IOC/FOK, execution reports | ⏳ |
| 3 | Deterministic replay from file → trade log | ⏳ |
| 4 | Benchmarks (throughput, p50/p99 latency) and optimization | ⏳ |

## Build & run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/demo
ctest --test-dir build --output-on-failure
```

With Visual Studio on Windows, executables land in `build\Debug\` (or `Release\`) and
`ctest` needs `-C Debug`.

GoogleTest is used if installed; otherwise CMake downloads it on first configure.
Pass `-DORDERBOOK_BUILD_TESTS=OFF` to build only the library and demo.

## Matching rules

- **Price priority:** better prices fill first (highest bid, lowest ask).
- **Time priority:** at the same price, earlier orders fill first (FIFO).
- **Execution price:** trades happen at the **resting** order's price. An incoming
  buy limit of 105 against a resting ask at 100 trades at 100.
- **Partial fills keep priority:** a resting order that partially fills stays at the
  front of its queue.
- A limit order matches as far as its limit allows; any remainder rests in the book
  at its own limit price.

## API

```cpp
AddResult add_limit(OrderId id, Side side, Price price, Quantity quantity);
bool      cancel(OrderId id);

std::optional<Price> best_bid() const;
std::optional<Price> best_ask() const;
Quantity             volume_at(Side side, Price price) const;
std::size_t          level_count(Side side) const;
std::size_t          order_count() const;
bool                 contains(OrderId id) const;
```

- `add_limit` validates the order, matches it against the opposite side, and rests
  any remainder. `AddResult` holds the status, the trades generated (in execution
  order), and the quantity left resting (`0` if fully filled or rejected).
- `cancel` removes a resting order, returning `false` if it isn't in the book
  (never existed, already cancelled, or already fully filled).
- `best_bid` / `best_ask` are empty when that side of the book is empty.
- Each `Trade` records the aggressor (incoming) order, the resting order, the
  aggressor's side, the price, and the quantity.

### Rejections

| Status | When |
|---|---|
| `RejectedInvalidQuantity` | quantity is 0 |
| `RejectedInvalidPrice` | price ≤ 0 |
| `RejectedDuplicateId` | an order with this ID is currently resting |

A rejected order never trades or changes the book.

## Design

| Component | Structure | Why |
|---|---|---|
| Prices | `int64_t` ticks (e.g. $100.50 → `10050`) | Avoids floating-point comparison errors |
| Each side | `std::map<Price, Level>`: bids `std::greater`, asks `std::less` | `begin()` is always the best price on both sides |
| Each level | `std::list<Order>` + running total | FIFO queue; O(1) removal from the middle |
| Order index | `unordered_map<OrderId, {side, price, list iterator}>` | Cancel without scanning the queue |

**Single matching path for both sides.** Buys and sells differ only in when a price
crosses (a buy crosses asks ≤ its limit; a sell crosses bids ≥ its limit). That check
is passed into one templated `match()` as a lambda, so there's no duplicated
side-specific matching code.

**Stable iterators.** `std::list` iterators remain valid when other elements are
inserted or erased, so the iterator saved when an order rests can be used later to
unlink it in O(1).

**Invariants**, maintained after every operation:
- No empty price levels: a level is erased as soon as its last order fills or is
  cancelled, so `best_bid()` / `best_ask()` never report a price with nothing behind it.
- Each level's `total` equals the sum of its orders' remaining quantity.
- The book is never crossed or locked: an order only rests once the best opposite
  price no longer crosses its limit, so best bid < best ask whenever both exist.

### Complexity

| Operation | Complexity |
|---|---|
| Add (no cross) | O(log P) |
| Match | O(log P) per level touched + O(1) per fill |
| Cancel | O(log P) to find level + O(1) unlink |
| Best bid/ask | O(1) |

*P = number of active price levels.*

## Tests

23 GoogleTest cases, each named after the rule it checks, grouped as:

- **OrderBook:** resting without trading, best-price tracking, same-price aggregation.
- **Matching:** full and partial fills, time priority within a level, price priority
  over time priority, execution at the resting price, multi-level sweeps, stopping at
  the limit, buy/sell symmetry.
- **Cancel:** removal, double/unknown cancel, cancelling mid-queue without disturbing
  others' priority, best-price updates, cancelling after a partial fill.
- **Validation:** each rejection reason, and that rejected orders never trade.

The demo (`src/main.cpp`) replays a book where a buy for 200 @ 101.00 sweeps two ask
levels, producing fills of 100 and 50 at 100.50 and 50 at 101.00.

## Known limitations / roadmap

- **Duplicate IDs:** only live orders are checked, so a fully filled order's ID could
  be reused. Real venues reject any reused ID within a session; this needs an ID registry.
- **Cancel is O(log P):** the `Locator` stores the price, so cancel searches the map to
  reach the level. Caching the level's `std::map` iterator (safe, since map iterators
  survive other insertions and erasures) would make cancel O(1).
- **No self-trade prevention:** orders carry no owner, so one participant can trade
  with itself.
- **Allocation:** `std::map` and `std::list` allocate per node and scatter data across
  memory. Stage 4 replaces them with a flat price-indexed array and pooled orders, with
  benchmarks to measure the difference.

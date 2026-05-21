# Design Tradeoffs

## Networking: epoll/kqueue vs. io_uring

We chose `epoll`/`kqueue` over `io_uring` because:
- Cross-platform support (macOS requires `kqueue`)
- Mature, well-understood semantics
- Easier to reason about in a single-author codebase

`io_uring` would reduce syscall count for small messages and is the right
next step for a production system targeting Linux ≥ 5.1.

## Storage: sharded std::unordered_map vs. lock-free map

**Sharded shared_mutex**: predictable worst-case, easy to reason about,
compatible with the LRU list (which also lives per-shard). Lock contention
is low once shard count ≥ CPU count.

**Lock-free alternative** (e.g., Intel TBB `concurrent_hash_map`):
higher peak throughput under extreme concurrency, but complicates the
LRU integration and adds a heavyweight dependency.

The 16-shard design gives ~87.5% reduction in contention vs. a single
global lock with 8 concurrent writers.

## LRU eviction: doubly-linked list vs. clock/second-chance

`std::list` gives true O(1) touch/remove when the iterator is stored on
the entry — no scanning on eviction. The tradeoff is pointer chasing
(poor cache locality) vs. the clock algorithm's array-of-bits (great
locality, O(n) worst-case eviction).

For a cache at 1M entries the LRU list fit in ~40 MB, which is acceptable.
A clock eviction would be worthwhile above ~10M entries.

## TTL: lazy deletion vs. eager deletion

Lazy deletion (check expiry on `get()`) keeps the write path fast: `set()`
with TTL just stores the deadline. The background sweep (`sweep_expired()`)
runs every second to bound memory growth from dead keys.

Eager deletion would require a priority queue ordered by deadline, adding
O(log n) overhead to every `expire()` call and complicating the shard-lock
protocol.

## Persistence: separate RDB + AOF vs. WAL

Redis-compatible systems traditionally offer both. We implemented RDB for
point-in-time recovery (safe crash behavior) and AOF for durability between
snapshots. A unified WAL (à la LevelDB) would be simpler but harder to
tune for mixed read-heavy workloads.

## Thread model: event loop + inline dispatch vs. worker pool

Commands are dispatched inline in the event loop thread to keep latency
deterministic and avoid cross-thread synchronization overhead for the
common case (small payloads). The `ThreadPool` is initialized and wired up
but can be activated for CPU-intensive operations (e.g., `KEYS *` on a
large dataset) with a one-line change in `process_commands()`.

## Protocol: RESP2 vs. RESP3

RESP2 is sufficient for all Redis 2.x-compatible clients (`redis-cli`,
`redis-py`, `lettuce`, etc.). RESP3 adds typed maps and push events —
useful for Pub/Sub and streaming but out of scope for this implementation.

## Memory layout

Each `Entry` is ~56 bytes on x86-64 (string SSO + int64 + void*).
The `unordered_map` itself adds ~50–80 bytes of overhead per bucket.
At 1M keys: ~136 MB for the store, plus ~8 MB for the LRU list nodes.
A custom arena allocator would cut this by 30–40% by eliminating
per-allocation headers and improving spatial locality.

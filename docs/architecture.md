# Architecture Overview

## High-level design

```
                    ┌─────────────────────────────────────────┐
                    │              cacheserver                 │
                    │                                          │
  TCP clients  ──►  │  EventLoop (epoll/kqueue)                │
  (RESP protocol)   │    │                                     │
                    │    ├── TcpListener  ── accept()          │
                    │    │                                      │
                    │    └── Connection[]                       │
                    │         ├── Buffer (in/out)              │
                    │         └── RespParser                   │
                    │              │ parsed command             │
                    │              ▼                            │
                    │         Dispatcher                        │
                    │              │ route by name              │
                    │              ▼                            │
                    │        CommandHandler                     │
                    │              │                            │
                    │              ▼                            │
                    │           Store (16 shards)              │
                    │              │                            │
                    │        ┌─────┴──────┐                    │
                    │        │            │                     │
                    │     LruList     ExpiryMap                │
                    │                                          │
                    │  Persistence layer (background)          │
                    │    ├── Rdb (RDB-style snapshots)        │
                    │    └── Aof (append-only log)            │
                    │                                          │
                    │  Metrics HTTP server (port 9999)        │
                    │    └── /metrics  /health  /stats        │
                    └─────────────────────────────────────────┘
```

## Component breakdown

### EventLoop

Wraps `epoll` (Linux) or `kqueue` (macOS) behind a uniform interface.
Registered on edge-triggered mode for minimal syscall overhead.
Drives two callbacks per connection: `on_readable` and `on_writable`.

Write interest (`EPOLLOUT` / `EVFILT_WRITE`) is only registered when the
connection has buffered data to flush, keeping the event set sparse.

### Connection

Each TCP client gets a `Connection` object with:
- **in_buf**: receives raw bytes from `recv()`, fed into the RESP parser
- **out_buf**: receives serialized responses, flushed by `on_writable()`
- A `std::mutex` guarding `out_buf` so worker threads can write responses
  without racing with the flush path

### RespParser

Stateless, zero-copy parser. Works on `std::string_view` slices of
`in_buf.readable()` and returns `(status, value, consumed)` without
allocating until a complete value is assembled. Handles partial reads by
returning `ParseStatus::Incomplete` — the caller retains all unread bytes.

### Store

16-shard concurrent hash map (`std::unordered_map` + `std::shared_mutex`
per shard). The shard index is `hash(key) % 16`.

- **Reads** use `shared_lock` for read-heavy workloads.
- **Writes** upgrade to `unique_lock` only for the affected shard.
- Each shard owns a `LruList` (doubly-linked list), updated on every
  access. Entries store a heap-allocated iterator into the list for
  O(1) touch/remove without a second hash lookup.

### Persistence

**RDB**: Binary snapshot written atomically (tmp file + rename).
Triggered every `rdb_interval_secs` seconds by the maintenance thread,
and on clean shutdown.

**AOF**: Each write command is appended in RESP wire format.
On startup the AOF is replayed by re-parsing and re-dispatching each entry.

### Metrics

Lock-free atomic counters (relaxed ordering — approximate is fine for
monitoring). The EMA of latency uses `std::atomic<int64_t>` with a
×100 fixed-point encoding to avoid floating-point atomics.

The Prometheus endpoint `/metrics` emits text in exposition format
consumable by any Prometheus scraper.

## Threading model

```
Main thread:        EventLoop::run()  ← epoll/kqueue
                      │ accept() → new Connection
                      │ recv()  → RespParser → Dispatcher
                      │ send()  → flush out_buf

Maintenance thread: sweep_expired() | rdb.save() | metrics.update()

(ThreadPool is wired in but command dispatch currently runs inline
 in the event loop thread for simplicity. Moving it to the pool is
 a one-line change in Connection::process_commands().)
```

## Data flow for a SET command

```
1. Client sends:  *3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
2. EventLoop detects EPOLLIN on the client fd
3. Connection::on_readable() calls recv() → in_buf
4. RespParser::parse() returns ParseStatus::Ok
5. Dispatcher::dispatch("set", store, req) → handle_set()
6. Store::set("foo", "bar") under shard lock
7. RespSerializer::ok() → "+OK\r\n" → out_buf
8. EventLoop detects out_buf non-empty → registers EPOLLOUT
9. Connection::on_writable() flushes out_buf via send()
```

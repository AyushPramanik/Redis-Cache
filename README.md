# cacheserver

A high-performance, Redis-compatible in-memory cache server built in C++20.
Implements the RESP2 protocol, supports core Redis commands, and is designed
for low latency under high concurrency.

## Features

- **RESP2 protocol** — drop-in compatible with `redis-cli` and any Redis client
- **Supported commands**: `SET` (with EX/PX/NX/XX), `GET`, `DEL`, `EXISTS`,
  `EXPIRE`, `PEXPIRE`, `TTL`, `PTTL`, `INCR`, `INCRBY`, `KEYS`, `DBSIZE`,
  `FLUSHALL`, `PING`
- **Sharded storage** — 16-shard `unordered_map` + per-shard `shared_mutex`
  for low contention under concurrent load
- **LRU eviction** — O(1) touch/evict via doubly-linked list + inline iterator
- **Lazy TTL expiry** + periodic background sweep
- **RDB snapshots** — atomic write-rename, configurable interval
- **AOF logging** — RESP-format append log with startup replay
- **Prometheus metrics** — `/metrics`, `/stats`, `/health` on port 9999
- **epoll** (Linux) / **kqueue** (macOS) event loop, edge-triggered
- **TCP_NODELAY** for minimal per-packet latency
- Config file (INI-style) + CLI flag overrides
- React + TypeScript admin dashboard (Recharts)
- Docker + docker-compose
- GitHub Actions CI (Linux + macOS, Debug + Release)

## Quick start

### Build (requires CMake ≥ 3.20, C++20 compiler)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/cacheserver --config config/server.conf
```

### Docker

```bash
docker compose up --build
# Server on :6399, metrics on :9999, dashboard on :5173
```

### Connect with redis-cli

```bash
redis-cli -p 6399
127.0.0.1:6399> PING
PONG
127.0.0.1:6399> SET greeting "hello" EX 60
OK
127.0.0.1:6399> GET greeting
"hello"
127.0.0.1:6399> TTL greeting
(integer) 59
127.0.0.1:6399> INCR visits
(integer) 1
```

## Configuration

```ini
# config/server.conf
host             = 0.0.0.0
port             = 6399
worker_threads   = 4
max_keys         = 1000000
rdb_enabled      = true
rdb_interval_secs = 300
aof_enabled      = false
metrics_port     = 9999
log_level        = info
```

CLI overrides: `--port 6399 --workers 8 --log-level debug`

## Running tests

```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel
cd build-debug && ctest --output-on-failure
```

## Benchmarks

Run the internal C++ microbenchmark:

```bash
./scripts/benchmark.sh 500000
```

Sample results on Apple M2 Pro (Release build, 8 threads):

```
[SET (single-thread)]
  throughput :  2,847,391 ops/sec
  p50 latency:  0 μs
  p99 latency:  1 μs
  p999 latency: 3 μs

[GET (single-thread, random)]
  throughput :  4,103,204 ops/sec
  p50 latency:  0 μs
  p99 latency:  1 μs
  p999 latency: 2 μs

[GET (8 threads concurrent)]
  throughput :  18,942,301 ops/sec
  p50 latency:  0 μs
  p99 latency:  2 μs
  p999 latency: 6 μs
```

Network benchmark via `redis-benchmark` against a local instance:

```
PING:   320,000 req/sec
SET:    280,000 req/sec  (pipeline=16)
GET:    310,000 req/sec  (pipeline=16)
INCR:   275,000 req/sec
```

## Metrics

Prometheus-compatible exposition at `http://localhost:9999/metrics`:

```
cache_commands_total 1048576
cache_hits_total 921234
cache_misses_total 127342
cache_active_connections 42
cache_key_count 384291
cache_memory_bytes 52428800
cache_avg_latency_us 0.34
cache_hit_rate 0.8785
cache_commands_per_sec 284109
```

JSON snapshot at `/stats`, health check at `/health`.

## Architecture

See [docs/architecture.md](docs/architecture.md) for the full design diagram
and component breakdown.

See [docs/design_tradeoffs.md](docs/design_tradeoffs.md) for engineering
decisions and their rationale.

## Dashboard

```bash
cd dashboard
npm install
npm run dev
# Open http://localhost:5173
```

The dashboard polls `/api/stats` every 1.5 seconds and shows:
- Active connections, key count, memory usage
- Commands/sec and hit rate over a 90-second rolling window
- Average latency trend

## Project structure

```
src/
  server/       Server coordinator, Config
  network/      EventLoop (epoll/kqueue), Connection, Buffer, TcpListener
  protocol/     RespParser, RespSerializer, RespTypes
  storage/      Store (sharded), LruEviction, Entry
  commands/     Dispatcher + per-command handlers
  persistence/  Rdb (snapshots), Aof (append-only log)
  metrics/      Metrics (lock-free counters), HttpServer
  concurrency/  ThreadPool
  util/         Logger, TimeUtils
tests/          Catch2 unit tests
benchmarks/     C++ microbenchmarks
dashboard/      React + Vite admin UI
scripts/        load_test.sh, benchmark.sh
docs/           Architecture, design tradeoffs
config/         server.conf example
```

## License

MIT

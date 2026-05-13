#!/usr/bin/env bash
# Load test using redis-benchmark (requires redis-tools installed).
# Usage: ./scripts/load_test.sh [host] [port] [num_requests]

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-6399}"
NUM_REQS="${3:-100000}"
PIPELINE="${4:-16}"

if ! command -v redis-benchmark &>/dev/null; then
    echo "redis-benchmark not found. Install via: brew install redis / apt install redis-tools"
    exit 1
fi

echo "=========================================="
echo "  cacheserver load test"
echo "  Host: $HOST:$PORT"
echo "  Requests: $NUM_REQS | Pipeline depth: $PIPELINE"
echo "=========================================="

run_bench() {
    local name="$1"
    shift
    echo ""
    echo "--- $name ---"
    redis-benchmark -h "$HOST" -p "$PORT" -n "$NUM_REQS" -P "$PIPELINE" -q "$@"
}

run_bench "PING"        -t ping
run_bench "SET"         -t set  -d 64
run_bench "GET"         -t get
run_bench "INCR"        -t incr
run_bench "MIXED"       -t set,get -d 64 --ratio 1:9

echo ""
echo "Load test complete."

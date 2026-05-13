#!/usr/bin/env bash
# Run the built-in C++ store microbenchmark.
# Usage: ./scripts/benchmark.sh [num_ops]

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
NUM_OPS="${1:-500000}"

if [[ ! -f "$BUILD_DIR/benchmarks/bench_store" ]]; then
    echo "Binary not found. Building first..."
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release .
    cmake --build "$BUILD_DIR" --target bench_store -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
fi

echo "Running bench_store with $NUM_OPS ops..."
"$BUILD_DIR/benchmarks/bench_store" "$NUM_OPS"

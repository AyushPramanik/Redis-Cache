# Build stage
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=g++ \
    && cmake --build build --parallel "$(nproc)" --target cacheserver

# Runtime stage — minimal image
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /src/build/cacheserver /app/cacheserver
COPY config/server.conf /app/config/server.conf

EXPOSE 6399 9999

HEALTHCHECK --interval=5s --timeout=3s --start-period=5s --retries=3 \
    CMD bash -c "echo PING | nc -q1 localhost 6399 | grep -q PONG" || exit 1

ENTRYPOINT ["/app/cacheserver", "--config", "/app/config/server.conf"]

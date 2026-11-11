# ==============================================================================
# Stage 1: Native C++20 LTO Hardened Builder
# ==============================================================================
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    make \
    ca-certificates \
    libsqlite3-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY include ./include
COPY third_party ./third_party
COPY src ./src
COPY tests ./tests
COPY Makefile ./

RUN make -j"$(nproc)" release && make -j"$(nproc)" test

# ==============================================================================
# Stage 2: Minimal Hardened Runtime Container
# ==============================================================================
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    tzdata \
    gosu \
    curl \
    libsqlite3-0 \
    libcurl4 \
    libssl3 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /opt/alist/data

WORKDIR /opt/alist
COPY --from=builder /src/alist /opt/alist/alist
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /opt/alist/alist /entrypoint.sh

ENV PUID=0 \
    PGID=0 \
    UMASK=022 \
    ALIST_PORT=5244

EXPOSE 5244
VOLUME ["/opt/alist/data"]

HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
    CMD curl -fsS http://127.0.0.1:${ALIST_PORT}/ping || exit 1

ENTRYPOINT ["/entrypoint.sh"]

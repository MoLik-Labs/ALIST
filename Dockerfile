# ==============================================================================
# Stage 1: Native C++20 builder and zero-CDN Web UI bundle
# ==============================================================================
FROM debian:bookworm-slim AS builder
ARG BUILD_JOBS=2

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    make \
    nodejs \
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
COPY web ./web
COPY scripts ./scripts
COPY Makefile ./

RUN node scripts/build_webui.mjs && \
    make -j"${BUILD_JOBS}" release && \
    make -j"${BUILD_JOBS}" test

# ==============================================================================
# Stage 2: Minimal hardened runtime container
# ==============================================================================
FROM debian:bookworm-slim

LABEL org.opencontainers.image.title="Kova" \
      org.opencontainers.image.description="Kova Native C++20 Multi-Storage Workspace" \
      org.opencontainers.image.authors="MoLik Labs" \
      org.opencontainers.image.source="https://github.com/MoLik-Labs/Kova-INC" \
      org.opencontainers.image.version="4.2.0" \
      org.opencontainers.image.licenses="AGPL-3.0"

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

ENV PUID=10001 \
    PGID=10001 \
    UMASK=027 \
    ALIST_PORT=5244

EXPOSE 5244
VOLUME ["/opt/alist/data"]

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -fsS "http://127.0.0.1:${PORT:-${ALIST_PORT}}/ping" || exit 1

ENTRYPOINT ["/entrypoint.sh"]
CMD ["server"]

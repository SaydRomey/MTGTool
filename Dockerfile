FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    pkg-config \
    libsqlite3-dev \
    libcurl4-openssl-dev \
    liblzma-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
RUN make release

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    libsqlite3-0 \
    libcurl4 \
    liblzma5 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/build/bin/mtg /app/mtg
COPY --from=build /app/migrations /app/migrations
VOLUME ["/app/data"]
ENTRYPOINT ["/app/mtg"]
CMD ["help"]

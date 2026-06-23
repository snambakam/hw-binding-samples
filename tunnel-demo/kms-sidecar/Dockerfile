FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    autoconf \
    automake \
    build-essential \
    libssl-dev \
    libtss2-dev \
    libtool \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

RUN autoreconf -fi && ./configure && make -j"$(nproc)"

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    libtss2-fapi1 \
    tpm2-tools \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/kms-sidecar/kms-sidecar /usr/local/bin/kms-sidecar

ENV SOCKET_PATH=/run/kms/kms.sock
ENV BACKEND=dev
ENV POLICY_CLIENT_ID=sample-service
ENV POLICY_KEY_ID=server-key
ENV RATE_LIMIT_PER_MIN=120
ENV TPM_KEY_PATH=HS/SRK/kms-signing-key

CMD ["/usr/local/bin/kms-sidecar"]

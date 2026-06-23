FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    autoconf \
    automake \
    build-essential \
    libssl-dev \
    libtool \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

RUN autoreconf -fi && ./configure && make -j"$(nproc)"

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/sample-service/sample-service /usr/local/bin/sample-service

ENV SOCKET_PATH=/run/kms/kms.sock
ENV KMS_CLIENT_ID=sample-service
ENV KMS_KEY_ID=server-key
ENV SAMPLE_MESSAGE=hello-from-sample

CMD ["/usr/local/bin/sample-service"]

FROM fedora:44

# Runtime libraries only. The binary is built on the host (see build/) and
# copied in below; no toolchain or build steps live in this image.
RUN dnf install -y --setopt=install_weak_deps=False \
    openssl-libs \
    grpc-cpp \
    protobuf \
    && dnf clean all

COPY build/sample-service/sample-service /usr/local/bin/sample-service

ENV SOCKET_PATH=/run/kms/kms.sock
ENV KMS_CLIENT_ID=sample-service
ENV KMS_KEY_ID=server-key
ENV SAMPLE_MESSAGE=hello-from-sample

CMD ["/usr/local/bin/sample-service"]

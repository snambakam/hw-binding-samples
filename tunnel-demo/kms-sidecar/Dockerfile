FROM fedora:44

# Runtime libraries only. The binary is built on the host (see build/) and
# copied in below; no toolchain or build steps live in this image.
RUN dnf install -y --setopt=install_weak_deps=False \
    openssl-libs \
    tpm2-tss \
    tpm2-tools \
    grpc-cpp \
    protobuf \
    && dnf clean all

COPY build/kms-sidecar/kms-sidecar /usr/local/bin/kms-sidecar

ENV SOCKET_PATH=/run/kms/kms.sock
ENV BACKEND=tpm
ENV POLICY_CLIENT_ID=sample-service
ENV POLICY_KEY_ID=server-key
ENV RATE_LIMIT_PER_MIN=120
ENV TPM_KEY_PATH=HS/SRK/kms-signing-key
ENV TPM_PUBLIC_PEM=/keys/server-public.pem
# FAPI keystore is bind-mounted at /tpm at run time (see run-containers.sh).
ENV TSS2_FAPICONF=/tpm/fapi-config.container.json

CMD ["/usr/local/bin/kms-sidecar"]

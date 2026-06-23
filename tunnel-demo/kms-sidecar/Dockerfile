FROM fedora:44

# Runtime libraries only. The binary is built on the host (see build/) and
# copied in below; no toolchain or build steps live in this image.
RUN dnf install -y --setopt=install_weak_deps=False \
    openssl-libs \
    tpm2-tss \
    tpm2-tools \
    && dnf clean all

COPY build/kms-sidecar/kms-sidecar /usr/local/bin/kms-sidecar

ENV SOCKET_PATH=/run/kms/kms.sock
ENV BACKEND=dev
ENV POLICY_CLIENT_ID=sample-service
ENV POLICY_KEY_ID=server-key
ENV RATE_LIMIT_PER_MIN=120
ENV TPM_KEY_PATH=HS/SRK/kms-signing-key

CMD ["/usr/local/bin/kms-sidecar"]

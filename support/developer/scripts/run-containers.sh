#!/usr/bin/env bash
#
# run-containers.sh
#
# Builds the runtime container images from prebuilt host binaries and runs the
# TPM-backed KMS sidecar together with the sample service, sharing the sidecar
# Unix socket through a named volume.
#
# The container engine defaults to podman; override with ENGINE=docker.
#
# Usage:
#   ./support/developer/scripts/run-containers.sh
#
set -euo pipefail

ENGINE="${ENGINE:-podman}"
KMS_IMAGE="hw-kms-sidecar"
SAMPLE_IMAGE="hw-sample-service"
KMS_CONTAINER="kms-sidecar"
SAMPLE_CONTAINER="sample-service"
SOCKET_VOLUME="kms-socket"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "${REPO_ROOT}"

KMS_BIN="build/kms-sidecar/kms-sidecar"
SAMPLE_BIN="build/sample-service/sample-service"

if ! command -v "${ENGINE}" >/dev/null 2>&1; then
    echo "Container engine '${ENGINE}' not found. Set ENGINE=docker or install podman." >&2
    exit 1
fi

if [[ ! -x "${KMS_BIN}" || ! -x "${SAMPLE_BIN}" ]]; then
    echo "Prebuilt binaries not found. Building them first..."
    mkdir -p build
    (cd build && ./bootstrap.sh && make -j"$(nproc)")
fi

echo "Building images with ${ENGINE}..."
"${ENGINE}" build -t "${KMS_IMAGE}" -f tunnel-demo/kms-sidecar/Dockerfile .
"${ENGINE}" build -t "${SAMPLE_IMAGE}" -f tunnel-demo/sample-service/Dockerfile .

echo "Cleaning up any previous run..."
"${ENGINE}" rm -f "${KMS_CONTAINER}" "${SAMPLE_CONTAINER}" >/dev/null 2>&1 || true
"${ENGINE}" volume rm "${SOCKET_VOLUME}" >/dev/null 2>&1 || true
"${ENGINE}" volume create "${SOCKET_VOLUME}" >/dev/null

echo "Starting ${KMS_CONTAINER}..."
"${ENGINE}" run -d \
    --name "${KMS_CONTAINER}" \
    -v "${SOCKET_VOLUME}:/run/kms" \
    "${KMS_IMAGE}" >/dev/null

echo "Waiting for the sidecar socket..."
for _ in $(seq 1 30); do
    if "${ENGINE}" exec "${KMS_CONTAINER}" test -S /run/kms/kms.sock >/dev/null 2>&1; then
        break
    fi
    sleep 0.5
done

echo "Running ${SAMPLE_CONTAINER}..."
set +e
"${ENGINE}" run --rm \
    --name "${SAMPLE_CONTAINER}" \
    -v "${SOCKET_VOLUME}:/run/kms" \
    "${SAMPLE_IMAGE}"
SAMPLE_RC=$?
set -e

echo "Stopping ${KMS_CONTAINER}..."
"${ENGINE}" rm -f "${KMS_CONTAINER}" >/dev/null 2>&1 || true

echo "sample-service exit code: ${SAMPLE_RC}"
exit "${SAMPLE_RC}"

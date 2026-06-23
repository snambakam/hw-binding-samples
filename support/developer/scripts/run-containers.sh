#!/usr/bin/env bash
#
# run-containers.sh
#
# Builds the runtime container images from prebuilt host binaries and runs the
# TPM-backed KMS sidecar together with the sample service, sharing the sidecar
# Unix socket through a named volume.
#
# By default the sidecar runs in TPM mode (BACKEND=tpm) and the FAPI keystore
# provisioned by provision-tpm.sh is bind-mounted into the container. Set
# BACKEND=dev to use the in-process RSA key instead (no TPM required).
#
# The container engine defaults to podman; override with ENGINE=docker.
#
# Usage:
#   ./support/developer/scripts/run-containers.sh
#   BACKEND=dev ./support/developer/scripts/run-containers.sh
#
set -euo pipefail

ENGINE="${ENGINE:-podman}"
BACKEND="${BACKEND:-tpm}"
KMS_IMAGE="hw-kms-sidecar"
SAMPLE_IMAGE="hw-sample-service"
KMS_CONTAINER="kms-sidecar"
SAMPLE_CONTAINER="sample-service"
SOCKET_VOLUME="kms-socket"

TPM_DEVICE="/dev/tpmrm0"
TPM_STATE_DIR="${TPM_STATE_DIR:-$HOME/.local/share/hw-binding-tpm}"
CONTAINER_FAPI_CONF="$TPM_STATE_DIR/fapi-config.container.json"

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

# Assemble sidecar run options. In TPM mode, pass the TPM device and bind-mount
# the provisioned FAPI keystore and the public-key directory.
KMS_RUN_OPTS=(-d --name "${KMS_CONTAINER}" -v "${SOCKET_VOLUME}:/run/kms" -e "BACKEND=${BACKEND}")
if [[ "${BACKEND}" == "tpm" ]]; then
    if [[ ! -c "${TPM_DEVICE}" ]]; then
        echo "BACKEND=tpm but ${TPM_DEVICE} is missing. Attach a TPM or run with BACKEND=dev." >&2
        exit 1
    fi
    if [[ ! -f "${CONTAINER_FAPI_CONF}" ]]; then
        echo "FAPI keystore not provisioned (${CONTAINER_FAPI_CONF} missing)." >&2
        echo "Run ./support/developer/scripts/provision-tpm.sh first, or use BACKEND=dev." >&2
        exit 1
    fi
    KMS_RUN_OPTS+=(
        --device "${TPM_DEVICE}"
        --security-opt label=disable
        -v "${TPM_STATE_DIR}:/tpm"
        -v "${REPO_ROOT}/keys:/keys:ro"
    )
fi

echo "Starting ${KMS_CONTAINER} (BACKEND=${BACKEND})..."
"${ENGINE}" run "${KMS_RUN_OPTS[@]}" "${KMS_IMAGE}" >/dev/null

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

if [[ "${SAMPLE_RC}" -ne 0 ]]; then
    echo "--- ${KMS_CONTAINER} logs ---"
    "${ENGINE}" logs "${KMS_CONTAINER}" 2>&1 | tail -20 || true
fi

echo "Stopping ${KMS_CONTAINER}..."
"${ENGINE}" rm -f "${KMS_CONTAINER}" >/dev/null 2>&1 || true

echo "sample-service exit code: ${SAMPLE_RC}"
exit "${SAMPLE_RC}"

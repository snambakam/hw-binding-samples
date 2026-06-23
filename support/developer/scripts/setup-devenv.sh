#!/usr/bin/env bash
#
# setup-devenv.sh
#
# Installs the packages required to build the TPM-backed KMS sidecar sample
# (C + Autotools) on a Fedora system.
#
# Usage:
#   ./support/developer/scripts/setup-devenv.sh
#
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    SUDO="sudo"
else
    SUDO=""
fi

PACKAGES=(
    autoconf
    automake
    libtool
    make
    gcc
    gcc-c++
    pkgconf-pkg-config
    openssl-devel
    tpm2-tools
    tpm2-tss-devel
    grpc-devel
    grpc-plugins
    protobuf-devel
    protobuf-compiler
)

echo "Installing build dependencies on Fedora..."
${SUDO} dnf install -y "${PACKAGES[@]}"

echo "Done. Installed:"
printf '  - %s\n' "${PACKAGES[@]}"

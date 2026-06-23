#!/usr/bin/env bash
#
# provision-tpm.sh
#
# Reusable, idempotent provisioning of a TPM-backed FAPI keystore for the
# kms-sidecar (BACKEND=tpm). It:
#   1. Ensures the current user can access the TPM resource-manager device.
#   2. Generates a dev FAPI config that uses an RSA profile and user-writable
#      keystore directories (so no root is needed for provisioning).
#   3. Provisions the FAPI keystore (skipped if already provisioned).
#   4. Creates the signing key (skipped if it already exists).
#   5. Exports the key's public RSA PEM for the sample-service to verify with.
#   6. Writes an env file you can `source` before running the sidecar.
#
# Re-running is safe; completed steps are detected and skipped.
#
# Usage:
#   ./support/developer/scripts/provision-tpm.sh
#   source ~/.local/share/hw-binding-tpm/tpm-env.sh   # then run the sidecar
#
# Override via environment:
#   TPM_KEY_PATH   FAPI key path        (default HS/SRK/kms-signing-key)
#   TPM_PROFILE    FAPI profile name    (default P_RSA2048SHA256)
#   TPM_TCTI       TCTI string          (default device:/dev/tpmrm0)
#   TPM_STATE_DIR  keystore/state dir   (default ~/.local/share/hw-binding-tpm)
#   TPM_PUBLIC_PEM public key PEM path  (default <repo>/keys/server-public.pem)
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

KEY_PATH="${TPM_KEY_PATH:-HS/SRK/kms-signing-key}"
PROFILE="${TPM_PROFILE:-P_RSA2048SHA256}"
TCTI="${TPM_TCTI:-device:/dev/tpmrm0}"
STATE_DIR="${TPM_STATE_DIR:-$HOME/.local/share/hw-binding-tpm}"
PUBLIC_PEM="${TPM_PUBLIC_PEM:-$REPO_ROOT/keys/server-public.pem}"

TPM_DEVICE="/dev/tpmrm0"
FAPI_CONF="$STATE_DIR/fapi-config.json"
ENV_FILE="$STATE_DIR/tpm-env.sh"
USER_KEYSTORE="$STATE_DIR/user-keystore"
SYSTEM_KEYSTORE="$STATE_DIR/system-keystore"
EVENTLOG_DIR="$STATE_DIR/eventlog"
PROFILE_DIR="$STATE_DIR/profiles"
PROVISIONED_MARKER="$STATE_DIR/.provisioned"

log() { printf '==> %s\n' "$*"; }

require_tool() {
    command -v "$1" >/dev/null 2>&1 || { echo "Required tool '$1' not found." >&2; exit 1; }
}

ensure_device_access() {
    if [[ ! -e "$TPM_DEVICE" ]]; then
        echo "No TPM device found at $TPM_DEVICE. Is a TPM attached to this host?" >&2
        exit 1
    fi
    if [[ -r "$TPM_DEVICE" && -w "$TPM_DEVICE" ]]; then
        log "TPM device $TPM_DEVICE is already accessible."
        return
    fi
    log "Granting access to $TPM_DEVICE (sudo password required)..."
    # usermod makes membership persistent across re-login; setfacl grants
    # immediate access for this session (the ACL is re-applied on each run).
    sudo sh -c "usermod -aG tss '$USER'; setfacl -m u:'$USER':rw '$TPM_DEVICE'"
    if [[ ! -r "$TPM_DEVICE" || ! -w "$TPM_DEVICE" ]]; then
        echo "Could not gain access to $TPM_DEVICE." >&2
        exit 1
    fi
}

write_fapi_profile() {
    mkdir -p "$PROFILE_DIR"
    # Copy of the stock RSA profile with two changes needed for a virtual/VM TPM
    # and our use case:
    #   - ek_cert_less: skip Endorsement Key certificate verification (no
    #     manufacturer EK cert is available on an emulated/VM TPM).
    #   - rsa_signing_scheme RSASSA: PKCS#1v1.5 to match Fapi_Sign("RSA_SSA")
    #     in the sidecar and the sample-service's PKCS#1v1.5 verification.
    cat > "$PROFILE_DIR/$PROFILE.json" <<'EOF'
{
    "type": "TPM2_ALG_RSA",
    "nameAlg":"TPM2_ALG_SHA256",
    "srk_template": "system,restricted,decrypt,0x81000001",
    "srk_description": "Storage root key SRK",
    "srk_persistent": 1,
    "ek_template":  "system,restricted,decrypt",
    "ek_description": "Endorsement key EK",
    "rsa_signing_scheme": {
        "scheme":"TPM2_ALG_RSASSA",
        "details":{
            "hashAlg":"TPM2_ALG_SHA256"
        }
    },
    "rsa_decrypt_scheme": {
        "scheme":"TPM2_ALG_OAEP",
        "details":{
            "hashAlg":"TPM2_ALG_SHA256"
        }
    },
    "sym_mode":"TPM2_ALG_CFB",
    "sym_parameters": {
        "algorithm":"TPM2_ALG_AES",
        "keyBits":"128",
        "mode":"TPM2_ALG_CFB"
    },
    "sym_block_size": 16,
    "pcr_selection": [
        { "hash": "TPM2_ALG_SHA1",
          "pcrSelect": [ ]
        },
        { "hash": "TPM2_ALG_SHA256",
          "pcrSelect": [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 ]
        }
    ],
    "exponent": 0,
    "keyBits": 2048,
    "session_symmetric":{
        "algorithm":"TPM2_ALG_AES",
        "keyBits":"128",
        "mode":"TPM2_ALG_CFB"
    },
    "ek_policy": {
        "description": "Endorsement hierarchy used for policy secret.",
        "policy":[
            {
                "type":"POLICYSECRET",
                "objectName": "4000000b"
            }
        ]
    }
}
EOF
    log "FAPI profile written to $PROFILE_DIR/$PROFILE.json (ek_cert_less, RSASSA)."
}

write_fapi_config() {
    mkdir -p "$USER_KEYSTORE" "$SYSTEM_KEYSTORE" "$EVENTLOG_DIR"
    cat > "$FAPI_CONF" <<EOF
{
    "profile_name": "$PROFILE",
    "profile_dir": "$PROFILE_DIR",
    "user_dir": "$USER_KEYSTORE",
    "system_dir": "$SYSTEM_KEYSTORE",
    "tcti": "$TCTI",
    "ek_cert_less": "yes",
    "system_pcrs": [],
    "log_dir": "$EVENTLOG_DIR",
    "firmware_log_file": "/dev/null",
    "ima_log_file": "/dev/null"
}
EOF
    export TSS2_FAPICONF="$FAPI_CONF"
    log "FAPI config written to $FAPI_CONF (profile $PROFILE)."
}

provision_keystore() {
    if [[ -f "$PROVISIONED_MARKER" ]]; then
        log "FAPI keystore already provisioned."
        return
    fi
    log "Provisioning FAPI keystore..."
    # Reset any partial keystore state from a previous failed attempt.
    rm -rf "$USER_KEYSTORE" "$SYSTEM_KEYSTORE"
    mkdir -p "$USER_KEYSTORE" "$SYSTEM_KEYSTORE"
    tss2 provision
    touch "$PROVISIONED_MARKER"
}

create_signing_key() {
    if tss2 list 2>/dev/null | grep -q "$KEY_PATH"; then
        log "Signing key $KEY_PATH already exists."
        return
    fi
    log "Creating signing key $KEY_PATH..."
    # Empty authValue so the sidecar can sign non-interactively (it registers no
    # FAPI auth callback). --type sign,noDa: unrestricted signing key.
    tss2 createkey --path "$KEY_PATH" --type "sign, noDa" --authValue ""
}

export_public_pem() {
    mkdir -p "$(dirname "$PUBLIC_PEM")"
    local digest="$STATE_DIR/.digest.bin"
    local throwaway_sig="$STATE_DIR/.throwaway.sig"
    head -c 32 /dev/urandom > "$digest"
    # Fapi_Sign returns the public key in PEM form; a throwaway signature is the
    # simplest way to extract it via the CLI.
    tss2 sign \
        --keyPath "$KEY_PATH" \
        --padding "RSA_SSA" \
        --digest "$digest" \
        --signature "$throwaway_sig" \
        --publicKey "$PUBLIC_PEM" \
        --force
    rm -f "$digest" "$throwaway_sig"
    log "Public key PEM written to $PUBLIC_PEM."
}

write_env_file() {
    cat > "$ENV_FILE" <<EOF
# Source this file before running the kms-sidecar in TPM mode:
#   source "$ENV_FILE"
#   ./build/kms-sidecar/kms-sidecar
export TSS2_FAPICONF="$FAPI_CONF"
export BACKEND="tpm"
export TPM_KEY_PATH="$KEY_PATH"
export TPM_PUBLIC_PEM="$PUBLIC_PEM"
EOF
    log "Environment file written to $ENV_FILE."
}

main() {
    require_tool tss2
    require_tool setfacl
    ensure_device_access
    write_fapi_profile
    write_fapi_config
    provision_keystore
    create_signing_key
    export_public_pem
    write_env_file

    cat <<EOF

TPM provisioning complete.

To run the sidecar against the TPM:
  source "$ENV_FILE"
  ./build/kms-sidecar/kms-sidecar

The sample-service verifies signatures using:
  $PUBLIC_PEM
EOF
}

main "$@"

# TPM-Backed KMS Sidecar Sample (C + Autotools)

This repository contains a C-only sample of a TPM-backed KMS sidecar pattern:

- `kms-sidecar`: key-management sidecar that performs signing operations
- `sample-service`: application container that uses the sidecar over a Unix socket

The app never reads private key material.

## Build locally with Autotools

```bash
autoreconf -fi
./configure
make -j"$(nproc)"
```

Binaries:

- `kms-sidecar/kms-sidecar`
- `sample-service/sample-service`

## Run with Docker Compose

```bash
docker compose build
docker compose up -d kms-sidecar
```

Run the sample client once:

```bash
docker compose run --rm sample-service
```

Expected output includes:

- `health response: OK HEALTH dev`
- `sign/verify demo success for key_id=server-key`

## TPM mode

Default mode is `dev` (ephemeral in-memory RSA key inside sidecar process).

To use TPM-backed signing:

1. Provide TPM key context at `./keys/server-key.ctx`
2. Provide matching public key PEM at `./keys/server-public.pem`
3. Set `BACKEND=tpm` for `kms-sidecar` in `docker-compose.yml`

The sidecar uses `tpm2_sign` for signing in TPM mode and keeps key usage behind the sidecar boundary.

## Sidecar protocol

The sample uses a small text protocol over Unix socket:

- `HEALTH`
- `SIGN <client_id> <key_id> <payload_base64>`
- `GET_PUBLIC_KEY <client_id> <key_id>`

Policy and rate limiting are enforced in the sidecar via:

- `POLICY_CLIENT_ID`
- `POLICY_KEY_ID`
- `RATE_LIMIT_PER_MIN`

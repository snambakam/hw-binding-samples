#ifndef KMS_SIDECAR_H
#define KMS_SIDECAR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * KMS sidecar error codes.
 *
 * Range: 0x00001000 - 0x00001FFF
 * A value of KMS_OK (0) indicates success; any non-zero value is a failure.
 */
#define KMS_OK 0x00000000U
#define KMS_ERR_CONFIG 0x00001001U
#define KMS_ERR_SOCKET_DIR 0x00001002U
#define KMS_ERR_SOCKET_CREATE 0x00001003U
#define KMS_ERR_SOCKET_BIND 0x00001004U
#define KMS_ERR_SOCKET_LISTEN 0x00001005U
#define KMS_ERR_KEYGEN 0x00001006U
#define KMS_ERR_CLIENT_READ 0x00001007U
#define KMS_ERR_BASE64_DECODE 0x00001008U
#define KMS_ERR_BASE64_ENCODE 0x00001009U
#define KMS_ERR_SIGN 0x0000100AU
#define KMS_ERR_TPM_SIGN 0x0000100BU
#define KMS_ERR_FILE_READ 0x0000100CU
#define KMS_ERR_POLICY_DENIED 0x0000100DU
#define KMS_ERR_RATE_LIMITED 0x0000100EU
#define KMS_ERR_INTERNAL 0x0000100FU
#define KMS_ERR_TPM_INIT 0x00001010U

uint32_t KmsSidecarInitialize(void);
uint32_t KmsSidecarRun(void);
void KmsSidecarFinalize(void);

/*
 * Service operations invoked by the gRPC server shim. Each returns KMS_OK on
 * success or a non-zero KMS error code on failure.
 */
uint32_t KmsServiceHealth(
    char *backend,
    size_t backendLength
);

uint32_t KmsServiceSign(
    const char *clientId,
    const char *keyId,
    const unsigned char *payload,
    size_t payloadLength,
    unsigned char *signature,
    size_t signatureCapacity,
    size_t *signatureLength
);

uint32_t KmsServiceGetPublicKey(
    const char *clientId,
    const char *keyId,
    unsigned char *publicKey,
    size_t publicKeyCapacity,
    size_t *publicKeyLength
);

/*
 * gRPC server entry point implemented in C++ (grpc_server.cc). Binds to the
 * given Unix socket path and serves until the process is terminated.
 */
uint32_t KmsGrpcServerRun(
    const char *socketPath
);

#ifdef __cplusplus
}
#endif

#endif

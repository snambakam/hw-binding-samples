#include "includes.h"

static uint32_t
KmsLoadConfiguration(
    void
);

static uint32_t
KmsEnsureSocketDirectory(
    const char *socketPath
);

static uint32_t
KmsCreateDevKey(
    void
);

static uint32_t
KmsSignDev(
    const unsigned char *payload,
    size_t payloadLength,
    unsigned char *signature,
    size_t signatureLength,
    size_t *signatureOutLength
);

static uint32_t
KmsSignTpm(
    const unsigned char *payload,
    size_t payloadLength,
    unsigned char *signature,
    size_t signatureLength,
    size_t *signatureOutLength
);

static uint32_t
KmsReadFile(
    const char *path,
    unsigned char *buffer,
    size_t bufferLength,
    size_t *outLength
);

static uint32_t
KmsCheckPolicy(
    const char *clientId,
    const char *keyId
);

static uint32_t
KmsCheckRateLimit(
    void
);

uint32_t
KmsSidecarInitialize(
    void
) {
    uint32_t status;

    status = KmsLoadConfiguration();
    if (status != KMS_OK) {
        return status;
    }

    if (strcmp(gKmsBackend, "dev") == 0) {
        status = KmsCreateDevKey();
        if (status != KMS_OK) {
            return status;
        }
    } else if (strcmp(gKmsBackend, "tpm") == 0) {
        if (Fapi_Initialize(&gKmsFapiContext, NULL) != TSS2_RC_SUCCESS) {
            gKmsFapiContext = NULL;
            return KMS_ERR_TPM_INIT;
        }
    }

    return KMS_OK;
}

uint32_t
KmsSidecarRun(
    void
) {
    uint32_t status;

    status = KmsEnsureSocketDirectory(gKmsSocketPath);
    if (status != KMS_OK) {
        return status;
    }

    unlink(gKmsSocketPath);

    return KmsGrpcServerRun(gKmsSocketPath);
}

void
KmsSidecarFinalize(
    void
) {
    unlink(gKmsSocketPath);

    if (gKmsDevPrivateKey != NULL) {
        EVP_PKEY_free(gKmsDevPrivateKey);
        gKmsDevPrivateKey = NULL;
    }

    if (gKmsFapiContext != NULL) {
        Fapi_Finalize(&gKmsFapiContext);
        gKmsFapiContext = NULL;
    }
}

static uint32_t
KmsLoadConfiguration(
    void
) {
    const char *value;

    value = getenv("SOCKET_PATH");
    if (value != NULL && value[0] != '\0') {
        strncpy(gKmsSocketPath, value, sizeof(gKmsSocketPath) - 1);
        gKmsSocketPath[sizeof(gKmsSocketPath) - 1] = '\0';
    }

    value = getenv("BACKEND");
    if (value != NULL && value[0] != '\0') {
        strncpy(gKmsBackend, value, sizeof(gKmsBackend) - 1);
        gKmsBackend[sizeof(gKmsBackend) - 1] = '\0';
    }

    value = getenv("POLICY_CLIENT_ID");
    if (value != NULL && value[0] != '\0') {
        strncpy(gKmsPolicyClientId, value, sizeof(gKmsPolicyClientId) - 1);
        gKmsPolicyClientId[sizeof(gKmsPolicyClientId) - 1] = '\0';
    }

    value = getenv("POLICY_KEY_ID");
    if (value != NULL && value[0] != '\0') {
        strncpy(gKmsPolicyKeyId, value, sizeof(gKmsPolicyKeyId) - 1);
        gKmsPolicyKeyId[sizeof(gKmsPolicyKeyId) - 1] = '\0';
    }

    value = getenv("RATE_LIMIT_PER_MIN");
    if (value != NULL && value[0] != '\0') {
        gKmsRateLimitPerMin = atoi(value);
        if (gKmsRateLimitPerMin <= 0) {
            gKmsRateLimitPerMin = KMS_DEFAULT_RATE_LIMIT_PER_MIN;
        }
    }

    value = getenv("TPM_KEY_PATH");
    if (value != NULL && value[0] != '\0') {
        strncpy(gKmsFapiKeyPath, value, sizeof(gKmsFapiKeyPath) - 1);
        gKmsFapiKeyPath[sizeof(gKmsFapiKeyPath) - 1] = '\0';
    }

    value = getenv("TPM_PUBLIC_PEM");
    if (value != NULL && value[0] != '\0') {
        strncpy(gKmsTpmPublicPem, value, sizeof(gKmsTpmPublicPem) - 1);
        gKmsTpmPublicPem[sizeof(gKmsTpmPublicPem) - 1] = '\0';
    }

    return KMS_OK;
}

static uint32_t
KmsEnsureSocketDirectory(
    const char *socketPath
) {
    char buffer[PATH_MAX];
    char *slash;

    strncpy(buffer, socketPath, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    slash = strrchr(buffer, '/');
    if (slash == NULL) {
        return KMS_ERR_SOCKET_DIR;
    }

    *slash = '\0';
    if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return KMS_ERR_SOCKET_DIR;
    }

    return KMS_OK;
}

static uint32_t
KmsCreateDevKey(
    void
) {
    EVP_PKEY_CTX *context;

    context = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (context == NULL) {
        return KMS_ERR_KEYGEN;
    }

    if (EVP_PKEY_keygen_init(context) <= 0) {
        EVP_PKEY_CTX_free(context);
        return KMS_ERR_KEYGEN;
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(context, 2048) <= 0) {
        EVP_PKEY_CTX_free(context);
        return KMS_ERR_KEYGEN;
    }

    if (EVP_PKEY_keygen(context, &gKmsDevPrivateKey) <= 0) {
        EVP_PKEY_CTX_free(context);
        return KMS_ERR_KEYGEN;
    }

    EVP_PKEY_CTX_free(context);
    return KMS_OK;
}

uint32_t
KmsServiceHealth(
    char *backend,
    size_t backendLength
) {
    if (backend == NULL || backendLength == 0) {
        return KMS_ERR_INTERNAL;
    }

    strncpy(backend, gKmsBackend, backendLength - 1);
    backend[backendLength - 1] = '\0';
    return KMS_OK;
}

uint32_t
KmsServiceSign(
    const char *clientId,
    const char *keyId,
    const unsigned char *payload,
    size_t payloadLength,
    unsigned char *signature,
    size_t signatureCapacity,
    size_t *signatureLength
) {
    uint32_t status;

    status = KmsCheckPolicy(clientId, keyId);
    if (status != KMS_OK) {
        return status;
    }

    status = KmsCheckRateLimit();
    if (status != KMS_OK) {
        return status;
    }

    if (strcmp(gKmsBackend, "tpm") == 0) {
        return KmsSignTpm(payload, payloadLength, signature, signatureCapacity, signatureLength);
    }

    return KmsSignDev(payload, payloadLength, signature, signatureCapacity, signatureLength);
}

uint32_t
KmsServiceGetPublicKey(
    const char *clientId,
    const char *keyId,
    unsigned char *publicKey,
    size_t publicKeyCapacity,
    size_t *publicKeyLength
) {
    uint32_t status;
    BIO *bio;
    int bioLength;

    status = KmsCheckPolicy(clientId, keyId);
    if (status != KMS_OK) {
        return status;
    }

    status = KmsCheckRateLimit();
    if (status != KMS_OK) {
        return status;
    }

    if (strcmp(gKmsBackend, "tpm") == 0) {
        return KmsReadFile(gKmsTpmPublicPem, publicKey, publicKeyCapacity, publicKeyLength);
    }

    bio = BIO_new(BIO_s_mem());
    if (bio == NULL) {
        return KMS_ERR_INTERNAL;
    }

    if (PEM_write_bio_PUBKEY(bio, gKmsDevPrivateKey) != 1) {
        BIO_free(bio);
        return KMS_ERR_INTERNAL;
    }

    bioLength = BIO_read(bio, publicKey, (int)publicKeyCapacity);
    BIO_free(bio);
    if (bioLength <= 0) {
        return KMS_ERR_INTERNAL;
    }

    *publicKeyLength = (size_t)bioLength;
    return KMS_OK;
}

static uint32_t
KmsSignDev(
    const unsigned char *payload,
    size_t payloadLength,
    unsigned char *signature,
    size_t signatureLength,
    size_t *signatureOutLength
) {
    EVP_MD_CTX *context;

    context = EVP_MD_CTX_new();
    if (context == NULL) {
        return KMS_ERR_SIGN;
    }

    if (EVP_DigestSignInit(context, NULL, EVP_sha256(), NULL, gKmsDevPrivateKey) <= 0) {
        EVP_MD_CTX_free(context);
        return KMS_ERR_SIGN;
    }

    if (EVP_DigestSignUpdate(context, payload, payloadLength) <= 0) {
        EVP_MD_CTX_free(context);
        return KMS_ERR_SIGN;
    }

    *signatureOutLength = signatureLength;
    if (EVP_DigestSignFinal(context, signature, signatureOutLength) <= 0) {
        EVP_MD_CTX_free(context);
        return KMS_ERR_SIGN;
    }

    EVP_MD_CTX_free(context);
    return KMS_OK;
}

static uint32_t
KmsSignTpm(
    const unsigned char *payload,
    size_t payloadLength,
    unsigned char *signature,
    size_t signatureLength,
    size_t *signatureOutLength
) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLength;
    uint8_t *fapiSignature;
    size_t fapiSignatureLength;
    TSS2_RC rc;

    if (gKmsFapiContext == NULL) {
        return KMS_ERR_TPM_SIGN;
    }

    if (EVP_Digest(payload, payloadLength, digest, &digestLength, EVP_sha256(), NULL) != 1) {
        return KMS_ERR_TPM_SIGN;
    }

    fapiSignature = NULL;
    fapiSignatureLength = 0;

    rc = Fapi_Sign(
        gKmsFapiContext,
        gKmsFapiKeyPath,
        "RSA_SSA",
        digest,
        (size_t)digestLength,
        &fapiSignature,
        &fapiSignatureLength,
        NULL,
        NULL
    );

    if (rc != TSS2_RC_SUCCESS) {
        return KMS_ERR_TPM_SIGN;
    }

    if (fapiSignature == NULL || fapiSignatureLength > signatureLength) {
        Fapi_Free(fapiSignature);
        return KMS_ERR_TPM_SIGN;
    }

    memcpy(signature, fapiSignature, fapiSignatureLength);
    *signatureOutLength = fapiSignatureLength;
    Fapi_Free(fapiSignature);
    return KMS_OK;
}

static uint32_t
KmsReadFile(
    const char *path,
    unsigned char *buffer,
    size_t bufferLength,
    size_t *outLength
) {
    FILE *file;

    file = fopen(path, "rb");
    if (file == NULL) {
        return KMS_ERR_FILE_READ;
    }

    *outLength = fread(buffer, 1, bufferLength, file);
    fclose(file);
    if (*outLength == 0) {
        return KMS_ERR_FILE_READ;
    }

    return KMS_OK;
}

static uint32_t
KmsCheckPolicy(
    const char *clientId,
    const char *keyId
) {
    if (strcmp(clientId, gKmsPolicyClientId) != 0) {
        return KMS_ERR_POLICY_DENIED;
    }

    if (strcmp(keyId, gKmsPolicyKeyId) != 0) {
        return KMS_ERR_POLICY_DENIED;
    }

    return KMS_OK;
}

static uint32_t
KmsCheckRateLimit(
    void
) {
    time_t now;
    int readIndex;
    int writeIndex;

    now = time(NULL);
    writeIndex = 0;

    for (readIndex = 0; readIndex < gKmsRateEventCount; readIndex++) {
        if ((now - gKmsRateEvents[readIndex]) < 60) {
            gKmsRateEvents[writeIndex++] = gKmsRateEvents[readIndex];
        }
    }

    gKmsRateEventCount = writeIndex;
    if (gKmsRateEventCount >= gKmsRateLimitPerMin || gKmsRateEventCount >= KMS_MAX_RATE_EVENTS) {
        return KMS_ERR_RATE_LIMITED;
    }

    gKmsRateEvents[gKmsRateEventCount++] = now;
    return KMS_OK;
}

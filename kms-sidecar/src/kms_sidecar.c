#include "includes.h"

static volatile sig_atomic_t gIsRunning = 1;

static void
KmsHandleSignal(
    int signalNumber
);

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
KmsHandleClient(
    int clientFd
);

static uint32_t
KmsProcessCommand(
    const char *line,
    char *response,
    size_t responseLength
);

static uint32_t
KmsDecodeBase64(
    const char *input,
    unsigned char *output,
    size_t outputLength,
    size_t *decodedLength
);

static uint32_t
KmsEncodeBase64(
    const unsigned char *input,
    size_t inputLength,
    char *output,
    size_t outputLength
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
    struct sigaction action;
    uint32_t status;

    status = KmsLoadConfiguration();
    if (status != KMS_OK) {
        return status;
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = KmsHandleSignal;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

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
    struct sockaddr_un address;
    uint32_t status;

    status = KmsEnsureSocketDirectory(gKmsSocketPath);
    if (status != KMS_OK) {
        return status;
    }

    gKmsServerFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (gKmsServerFd < 0) {
        perror("socket");
        return KMS_ERR_SOCKET_CREATE;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, gKmsSocketPath, sizeof(address.sun_path) - 1);

    unlink(gKmsSocketPath);
    if (bind(gKmsServerFd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        perror("bind");
        return KMS_ERR_SOCKET_BIND;
    }

    if (listen(gKmsServerFd, 16) != 0) {
        perror("listen");
        return KMS_ERR_SOCKET_LISTEN;
    }

    while (gIsRunning) {
        int clientFd = accept(gKmsServerFd, NULL, NULL);
        if (clientFd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }

        KmsHandleClient(clientFd);
        close(clientFd);
    }

    return KMS_OK;
}

void
KmsSidecarFinalize(
    void
) {
    if (gKmsServerFd >= 0) {
        close(gKmsServerFd);
        gKmsServerFd = -1;
    }

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

static void
KmsHandleSignal(
    int signalNumber
) {
    (void)signalNumber;
    gIsRunning = 0;
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

static uint32_t
KmsHandleClient(
    int clientFd
) {
    char input[KMS_MAX_LINE];
    char response[KMS_MAX_B64];
    ssize_t bytesRead;
    size_t index;

    memset(input, 0, sizeof(input));
    bytesRead = read(clientFd, input, sizeof(input) - 1);
    if (bytesRead <= 0) {
        return KMS_ERR_CLIENT_READ;
    }

    for (index = 0; index < (size_t)bytesRead; index++) {
        if (input[index] == '\n' || input[index] == '\r') {
            input[index] = '\0';
            break;
        }
    }

    if (KmsProcessCommand(input, response, sizeof(response)) != KMS_OK) {
        snprintf(response, sizeof(response), "ERR 500 internal_error\n");
    }

    write(clientFd, response, strlen(response));
    return KMS_OK;
}

static uint32_t
KmsProcessCommand(
    const char *line,
    char *response,
    size_t responseLength
) {
    char command[KMS_MAX_TEXT];
    char clientId[KMS_MAX_TEXT];
    char keyId[KMS_MAX_TEXT];
    char payloadB64[KMS_MAX_B64];

    unsigned char payload[KMS_MAX_B64];
    unsigned char signature[KMS_MAX_B64];
    unsigned char publicKey[KMS_MAX_B64];

    size_t payloadLength;
    size_t signatureLength;
    size_t publicKeyLength;
    char signatureB64[KMS_MAX_B64];
    char publicKeyB64[KMS_MAX_B64];

    memset(command, 0, sizeof(command));
    memset(clientId, 0, sizeof(clientId));
    memset(keyId, 0, sizeof(keyId));
    memset(payloadB64, 0, sizeof(payloadB64));

    if (sscanf(line, "%511s", command) != 1) {
        snprintf(response, responseLength, "ERR 400 invalid_command\n");
        return KMS_OK;
    }

    if (strcmp(command, "HEALTH") == 0) {
        snprintf(response, responseLength, "OK HEALTH %s\n", gKmsBackend);
        return KMS_OK;
    }

    if (strcmp(command, "GET_PUBLIC_KEY") == 0) {
        if (sscanf(line, "%511s %511s %511s", command, clientId, keyId) != 3) {
            snprintf(response, responseLength, "ERR 400 invalid_arguments\n");
            return KMS_OK;
        }

        if (KmsCheckPolicy(clientId, keyId) != KMS_OK) {
            snprintf(response, responseLength, "ERR 403 forbidden\n");
            return KMS_OK;
        }

        if (KmsCheckRateLimit() != KMS_OK) {
            snprintf(response, responseLength, "ERR 429 rate_limited\n");
            return KMS_OK;
        }

        if (strcmp(gKmsBackend, "tpm") == 0) {
            if (KmsReadFile(gKmsTpmPublicPem, publicKey, sizeof(publicKey), &publicKeyLength) != KMS_OK) {
                snprintf(response, responseLength, "ERR 404 public_key_not_found\n");
                return KMS_OK;
            }
        } else {
            BIO *bio = BIO_new(BIO_s_mem());
            if (bio == NULL) {
                snprintf(response, responseLength, "ERR 500 bio_error\n");
                return KMS_OK;
            }

            PEM_write_bio_PUBKEY(bio, gKmsDevPrivateKey);
            publicKeyLength = BIO_read(bio, publicKey, sizeof(publicKey));
            BIO_free(bio);
            if (publicKeyLength == 0 || publicKeyLength == (size_t)-1) {
                snprintf(response, responseLength, "ERR 500 public_key_error\n");
                return KMS_OK;
            }
        }

        if (KmsEncodeBase64(publicKey, publicKeyLength, publicKeyB64, sizeof(publicKeyB64)) != KMS_OK) {
            snprintf(response, responseLength, "ERR 500 base64_error\n");
            return KMS_OK;
        }

        snprintf(response, responseLength, "OK PUBLIC_KEY %s\n", publicKeyB64);
        return KMS_OK;
    }

    if (strcmp(command, "SIGN") == 0) {
        if (sscanf(line, "%511s %511s %511s %16383s", command, clientId, keyId, payloadB64) != 4) {
            snprintf(response, responseLength, "ERR 400 invalid_arguments\n");
            return KMS_OK;
        }

        if (KmsCheckPolicy(clientId, keyId) != KMS_OK) {
            snprintf(response, responseLength, "ERR 403 forbidden\n");
            return KMS_OK;
        }

        if (KmsCheckRateLimit() != KMS_OK) {
            snprintf(response, responseLength, "ERR 429 rate_limited\n");
            return KMS_OK;
        }

        if (KmsDecodeBase64(payloadB64, payload, sizeof(payload), &payloadLength) != KMS_OK) {
            snprintf(response, responseLength, "ERR 400 invalid_payload\n");
            return KMS_OK;
        }

        if (strcmp(gKmsBackend, "tpm") == 0) {
            if (KmsSignTpm(payload, payloadLength, signature, sizeof(signature), &signatureLength) != KMS_OK) {
                snprintf(response, responseLength, "ERR 500 sign_failed\n");
                return KMS_OK;
            }
        } else {
            if (KmsSignDev(payload, payloadLength, signature, sizeof(signature), &signatureLength) != KMS_OK) {
                snprintf(response, responseLength, "ERR 500 sign_failed\n");
                return KMS_OK;
            }
        }

        if (KmsEncodeBase64(signature, signatureLength, signatureB64, sizeof(signatureB64)) != KMS_OK) {
            snprintf(response, responseLength, "ERR 500 base64_error\n");
            return KMS_OK;
        }

        snprintf(response, responseLength, "OK SIGNATURE %s\n", signatureB64);
        return KMS_OK;
    }

    snprintf(response, responseLength, "ERR 400 unknown_command\n");
    return KMS_OK;
}

static uint32_t
KmsDecodeBase64(
    const char *input,
    unsigned char *output,
    size_t outputLength,
    size_t *decodedLength
) {
    int inputLength;
    int bytes;

    inputLength = (int)strlen(input);
    if (inputLength <= 0) {
        return KMS_ERR_BASE64_DECODE;
    }

    if (outputLength < (size_t)((inputLength / 4) * 3 + 3)) {
        return KMS_ERR_BASE64_DECODE;
    }

    bytes = EVP_DecodeBlock(output, (const unsigned char *)input, inputLength);
    if (bytes < 0) {
        return KMS_ERR_BASE64_DECODE;
    }

    while (input[inputLength - 1] == '=') {
        bytes--;
        inputLength--;
    }

    *decodedLength = (size_t)bytes;
    return KMS_OK;
}

static uint32_t
KmsEncodeBase64(
    const unsigned char *input,
    size_t inputLength,
    char *output,
    size_t outputLength
) {
    int encodedLength;

    if (outputLength < ((inputLength + 2) / 3) * 4 + 1) {
        return KMS_ERR_BASE64_ENCODE;
    }

    encodedLength = EVP_EncodeBlock((unsigned char *)output, input, (int)inputLength);
    if (encodedLength <= 0) {
        return KMS_ERR_BASE64_ENCODE;
    }

    output[encodedLength] = '\0';
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

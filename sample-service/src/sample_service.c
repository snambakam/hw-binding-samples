#include "includes.h"

static uint32_t
SampleLoadConfiguration(
    void
);

static uint32_t
SampleSendCommand(
    const char *command,
    char *response,
    size_t responseLength
);

static uint32_t
SampleEncodeBase64(
    const unsigned char *input,
    size_t inputLength,
    char *output,
    size_t outputLength
);

static uint32_t
SampleDecodeBase64(
    const char *input,
    unsigned char *output,
    size_t outputLength,
    size_t *decodedLength
);

static uint32_t
SampleExtractField(
    const char *line,
    const char *prefix,
    char *value,
    size_t valueLength
);

uint32_t
SampleServiceInitialize(
    void
) {
    return SampleLoadConfiguration();
}

uint32_t
SampleServiceRun(
    void
) {
    char request[SAMPLE_MAX_B64];
    char response[SAMPLE_MAX_B64];
    char payloadB64[SAMPLE_MAX_B64];
    char signatureB64[SAMPLE_MAX_B64];
    char publicKeyB64[SAMPLE_MAX_B64];

    unsigned char signature[SAMPLE_MAX_B64];
    unsigned char publicKey[SAMPLE_MAX_B64];

    size_t signatureLength;
    size_t publicKeyLength;

    EVP_PKEY *key;
    EVP_MD_CTX *verifyContext;
    BIO *bio;
    int verifyOk;

    if (SampleSendCommand("HEALTH\n", response, sizeof(response)) != SAMPLE_OK) {
        fprintf(stderr, "failed to call sidecar health\n");
        return SAMPLE_ERR_KMS_HEALTH;
    }

    printf("health response: %s", response);

    if (SampleEncodeBase64((const unsigned char *)gSampleMessage, strlen(gSampleMessage), payloadB64, sizeof(payloadB64)) != SAMPLE_OK) {
        return SAMPLE_ERR_BASE64_ENCODE;
    }

    snprintf(request, sizeof(request), "SIGN %s %s %s\n", gSampleClientId, gSampleKeyId, payloadB64);
    if (SampleSendCommand(request, response, sizeof(response)) != SAMPLE_OK) {
        fprintf(stderr, "failed to call sidecar sign\n");
        return SAMPLE_ERR_KMS_SIGN;
    }

    if (strncmp(response, "OK SIGNATURE ", 13) != 0) {
        fprintf(stderr, "unexpected sign response: %s", response);
        return SAMPLE_ERR_KMS_SIGN;
    }

    if (SampleExtractField(response, "OK SIGNATURE ", signatureB64, sizeof(signatureB64)) != SAMPLE_OK) {
        return SAMPLE_ERR_RESPONSE_FORMAT;
    }

    snprintf(request, sizeof(request), "GET_PUBLIC_KEY %s %s\n", gSampleClientId, gSampleKeyId);
    if (SampleSendCommand(request, response, sizeof(response)) != SAMPLE_OK) {
        fprintf(stderr, "failed to call sidecar public key\n");
        return SAMPLE_ERR_KMS_PUBLIC_KEY;
    }

    if (strncmp(response, "OK PUBLIC_KEY ", 14) != 0) {
        fprintf(stderr, "unexpected public key response: %s", response);
        return SAMPLE_ERR_KMS_PUBLIC_KEY;
    }

    if (SampleExtractField(response, "OK PUBLIC_KEY ", publicKeyB64, sizeof(publicKeyB64)) != SAMPLE_OK) {
        return SAMPLE_ERR_RESPONSE_FORMAT;
    }

    if (SampleDecodeBase64(signatureB64, signature, sizeof(signature), &signatureLength) != SAMPLE_OK) {
        fprintf(stderr, "invalid signature base64\n");
        return SAMPLE_ERR_BASE64_DECODE;
    }

    if (SampleDecodeBase64(publicKeyB64, publicKey, sizeof(publicKey), &publicKeyLength) != SAMPLE_OK) {
        fprintf(stderr, "invalid public key base64\n");
        return SAMPLE_ERR_BASE64_DECODE;
    }

    bio = BIO_new_mem_buf(publicKey, (int)publicKeyLength);
    if (bio == NULL) {
        return SAMPLE_ERR_INTERNAL;
    }

    key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (key == NULL) {
        fprintf(stderr, "failed to parse public key\n");
        return SAMPLE_ERR_PUBLIC_KEY_PARSE;
    }

    verifyContext = EVP_MD_CTX_new();
    if (verifyContext == NULL) {
        EVP_PKEY_free(key);
        return SAMPLE_ERR_INTERNAL;
    }

    if (EVP_DigestVerifyInit(verifyContext, NULL, EVP_sha256(), NULL, key) <= 0) {
        EVP_MD_CTX_free(verifyContext);
        EVP_PKEY_free(key);
        return SAMPLE_ERR_VERIFY;
    }

    if (EVP_DigestVerifyUpdate(verifyContext, gSampleMessage, strlen(gSampleMessage)) <= 0) {
        EVP_MD_CTX_free(verifyContext);
        EVP_PKEY_free(key);
        return SAMPLE_ERR_VERIFY;
    }

    verifyOk = EVP_DigestVerifyFinal(verifyContext, signature, signatureLength);
    EVP_MD_CTX_free(verifyContext);
    EVP_PKEY_free(key);

    if (verifyOk == 1) {
        printf("sign/verify demo success for key_id=%s\n", gSampleKeyId);
        return SAMPLE_OK;
    }

    fprintf(stderr, "signature verification failed\n");
    return SAMPLE_ERR_VERIFY;
}

void
SampleServiceFinalize(
    void
) {
}

static uint32_t
SampleLoadConfiguration(
    void
) {
    const char *value;

    value = getenv("SOCKET_PATH");
    if (value != NULL && value[0] != '\0') {
        strncpy(gSampleSocketPath, value, sizeof(gSampleSocketPath) - 1);
        gSampleSocketPath[sizeof(gSampleSocketPath) - 1] = '\0';
    }

    value = getenv("KMS_CLIENT_ID");
    if (value != NULL && value[0] != '\0') {
        strncpy(gSampleClientId, value, sizeof(gSampleClientId) - 1);
        gSampleClientId[sizeof(gSampleClientId) - 1] = '\0';
    }

    value = getenv("KMS_KEY_ID");
    if (value != NULL && value[0] != '\0') {
        strncpy(gSampleKeyId, value, sizeof(gSampleKeyId) - 1);
        gSampleKeyId[sizeof(gSampleKeyId) - 1] = '\0';
    }

    value = getenv("SAMPLE_MESSAGE");
    if (value != NULL && value[0] != '\0') {
        strncpy(gSampleMessage, value, sizeof(gSampleMessage) - 1);
        gSampleMessage[sizeof(gSampleMessage) - 1] = '\0';
    }

    return SAMPLE_OK;
}

static uint32_t
SampleSendCommand(
    const char *command,
    char *response,
    size_t responseLength
) {
    int clientFd;
    struct sockaddr_un address;
    ssize_t bytesRead;

    clientFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (clientFd < 0) {
        return SAMPLE_ERR_SOCKET_CONNECT;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, gSampleSocketPath, sizeof(address.sun_path) - 1);

    if (connect(clientFd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(clientFd);
        return SAMPLE_ERR_SOCKET_CONNECT;
    }

    if (write(clientFd, command, strlen(command)) != (ssize_t)strlen(command)) {
        close(clientFd);
        return SAMPLE_ERR_SOCKET_IO;
    }

    memset(response, 0, responseLength);
    bytesRead = read(clientFd, response, responseLength - 1);
    close(clientFd);
    if (bytesRead <= 0) {
        return SAMPLE_ERR_SOCKET_IO;
    }

    return SAMPLE_OK;
}

static uint32_t
SampleEncodeBase64(
    const unsigned char *input,
    size_t inputLength,
    char *output,
    size_t outputLength
) {
    int encodedLength;

    if (outputLength < ((inputLength + 2) / 3) * 4 + 1) {
        return SAMPLE_ERR_BASE64_ENCODE;
    }

    encodedLength = EVP_EncodeBlock((unsigned char *)output, input, (int)inputLength);
    if (encodedLength <= 0) {
        return SAMPLE_ERR_BASE64_ENCODE;
    }

    output[encodedLength] = '\0';
    return SAMPLE_OK;
}

static uint32_t
SampleDecodeBase64(
    const char *input,
    unsigned char *output,
    size_t outputLength,
    size_t *decodedLength
) {
    int inputLength;
    int bytes;

    inputLength = (int)strlen(input);
    if (inputLength <= 0) {
        return SAMPLE_ERR_BASE64_DECODE;
    }

    if (outputLength < (size_t)((inputLength / 4) * 3 + 3)) {
        return SAMPLE_ERR_BASE64_DECODE;
    }

    bytes = EVP_DecodeBlock(output, (const unsigned char *)input, inputLength);
    if (bytes < 0) {
        return SAMPLE_ERR_BASE64_DECODE;
    }

    while (input[inputLength - 1] == '=') {
        bytes--;
        inputLength--;
    }

    *decodedLength = (size_t)bytes;
    return SAMPLE_OK;
}

static uint32_t
SampleExtractField(
    const char *line,
    const char *prefix,
    char *value,
    size_t valueLength
) {
    size_t prefixLength;
    size_t lineLength;

    prefixLength = strlen(prefix);
    if (strncmp(line, prefix, prefixLength) != 0) {
        return SAMPLE_ERR_RESPONSE_FORMAT;
    }

    lineLength = strlen(line + prefixLength);
    while (lineLength > 0 && ((line[prefixLength + lineLength - 1] == '\n') || (line[prefixLength + lineLength - 1] == '\r'))) {
        lineLength--;
    }

    if (lineLength + 1 > valueLength) {
        return SAMPLE_ERR_RESPONSE_FORMAT;
    }

    memcpy(value, line + prefixLength, lineLength);
    value[lineLength] = '\0';
    return SAMPLE_OK;
}

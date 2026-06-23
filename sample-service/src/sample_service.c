#include "includes.h"

static uint32_t
SampleLoadConfiguration(
    void
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
    void *client;
    char backend[64];

    unsigned char signature[512];
    unsigned char publicKey[8192];

    size_t signatureLength;
    size_t publicKeyLength;

    EVP_PKEY *key;
    EVP_MD_CTX *verifyContext;
    BIO *bio;
    int verifyOk;
    uint32_t status;

    status = SampleKmsConnect(gSampleSocketPath, &client);
    if (status != SAMPLE_OK) {
        fprintf(stderr, "failed to connect to sidecar\n");
        return status;
    }

    status = SampleKmsHealth(client, backend, sizeof(backend));
    if (status != SAMPLE_OK) {
        fprintf(stderr, "failed to call sidecar health\n");
        SampleKmsDisconnect(client);
        return SAMPLE_ERR_KMS_HEALTH;
    }

    printf("health response: OK HEALTH %s\n", backend);

    status = SampleKmsSign(
        client,
        gSampleClientId,
        gSampleKeyId,
        (const unsigned char *)gSampleMessage,
        strlen(gSampleMessage),
        signature,
        sizeof(signature),
        &signatureLength
    );
    if (status != SAMPLE_OK) {
        fprintf(stderr, "failed to call sidecar sign\n");
        SampleKmsDisconnect(client);
        return SAMPLE_ERR_KMS_SIGN;
    }

    status = SampleKmsGetPublicKey(
        client,
        gSampleClientId,
        gSampleKeyId,
        publicKey,
        sizeof(publicKey),
        &publicKeyLength
    );
    if (status != SAMPLE_OK) {
        fprintf(stderr, "failed to call sidecar public key\n");
        SampleKmsDisconnect(client);
        return SAMPLE_ERR_KMS_PUBLIC_KEY;
    }

    SampleKmsDisconnect(client);

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

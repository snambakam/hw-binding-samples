#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>

#include "kms.grpc.pb.h"
#include "kms.pb.h"

extern "C" {
#include <sample_service.h>
}

namespace {

struct SampleKmsClient {
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<kms::KmsService::Stub> stub;
};

}  // namespace

extern "C" uint32_t
SampleKmsConnect(
    const char *target,
    void **outClient
) {
    if (target == NULL || outClient == NULL) {
        return SAMPLE_ERR_INTERNAL;
    }

    std::string address = std::string("unix:") + target;

    SampleKmsClient *client = new (std::nothrow) SampleKmsClient();
    if (client == NULL) {
        return SAMPLE_ERR_INTERNAL;
    }

    client->channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    client->stub = kms::KmsService::NewStub(client->channel);

    *outClient = client;
    return SAMPLE_OK;
}

extern "C" uint32_t
SampleKmsHealth(
    void *clientHandle,
    char *backend,
    size_t backendLength
) {
    if (clientHandle == NULL || backend == NULL || backendLength == 0) {
        return SAMPLE_ERR_INTERNAL;
    }

    SampleKmsClient *client = static_cast<SampleKmsClient *>(clientHandle);

    kms::HealthRequest request;
    kms::HealthResponse response;
    grpc::ClientContext context;

    grpc::Status status = client->stub->Health(&context, request, &response);
    if (!status.ok()) {
        return SAMPLE_ERR_KMS_HEALTH;
    }

    strncpy(backend, response.backend().c_str(), backendLength - 1);
    backend[backendLength - 1] = '\0';
    return SAMPLE_OK;
}

extern "C" uint32_t
SampleKmsSign(
    void *clientHandle,
    const char *clientId,
    const char *keyId,
    const unsigned char *payload,
    size_t payloadLength,
    unsigned char *signature,
    size_t signatureCapacity,
    size_t *signatureLength
) {
    if (clientHandle == NULL || signature == NULL || signatureLength == NULL) {
        return SAMPLE_ERR_INTERNAL;
    }

    SampleKmsClient *client = static_cast<SampleKmsClient *>(clientHandle);

    kms::SignRequest request;
    kms::SignResponse response;
    grpc::ClientContext context;

    request.set_client_id(clientId);
    request.set_key_id(keyId);
    request.set_payload(payload, payloadLength);

    grpc::Status status = client->stub->Sign(&context, request, &response);
    if (!status.ok()) {
        return SAMPLE_ERR_KMS_SIGN;
    }

    const std::string &signatureBytes = response.signature();
    if (signatureBytes.size() > signatureCapacity) {
        return SAMPLE_ERR_KMS_SIGN;
    }

    memcpy(signature, signatureBytes.data(), signatureBytes.size());
    *signatureLength = signatureBytes.size();
    return SAMPLE_OK;
}

extern "C" uint32_t
SampleKmsGetPublicKey(
    void *clientHandle,
    const char *clientId,
    const char *keyId,
    unsigned char *publicKey,
    size_t publicKeyCapacity,
    size_t *publicKeyLength
) {
    if (clientHandle == NULL || publicKey == NULL || publicKeyLength == NULL) {
        return SAMPLE_ERR_INTERNAL;
    }

    SampleKmsClient *client = static_cast<SampleKmsClient *>(clientHandle);

    kms::GetPublicKeyRequest request;
    kms::GetPublicKeyResponse response;
    grpc::ClientContext context;

    request.set_client_id(clientId);
    request.set_key_id(keyId);

    grpc::Status status = client->stub->GetPublicKey(&context, request, &response);
    if (!status.ok()) {
        return SAMPLE_ERR_KMS_PUBLIC_KEY;
    }

    const std::string &pemBytes = response.public_key_pem();
    if (pemBytes.size() > publicKeyCapacity) {
        return SAMPLE_ERR_KMS_PUBLIC_KEY;
    }

    memcpy(publicKey, pemBytes.data(), pemBytes.size());
    *publicKeyLength = pemBytes.size();
    return SAMPLE_OK;
}

extern "C" void
SampleKmsDisconnect(
    void *clientHandle
) {
    if (clientHandle == NULL) {
        return;
    }

    SampleKmsClient *client = static_cast<SampleKmsClient *>(clientHandle);
    delete client;
}

#include <grpcpp/grpcpp.h>

#include <string>
#include <vector>

#include "kms.grpc.pb.h"
#include "kms.pb.h"

extern "C" {
#include <kms_sidecar.h>
}

namespace {

grpc::Status
KmsMapStatus(
    uint32_t status
) {
    if (status == KMS_OK) {
        return grpc::Status::OK;
    }

    char message[64];
    snprintf(message, sizeof(message), "kms error 0x%08X", status);

    switch (status) {
        case KMS_ERR_POLICY_DENIED:
            return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, message);
        case KMS_ERR_RATE_LIMITED:
            return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, message);
        default:
            return grpc::Status(grpc::StatusCode::INTERNAL, message);
    }
}

class KmsServiceImpl final : public kms::KmsService::Service {
public:
    grpc::Status Health(
        grpc::ServerContext *context,
        const kms::HealthRequest *request,
        kms::HealthResponse *response
    ) override {
        (void)context;
        (void)request;

        char backend[64];
        uint32_t status = KmsServiceHealth(backend, sizeof(backend));
        if (status != KMS_OK) {
            return KmsMapStatus(status);
        }

        response->set_backend(backend);
        return grpc::Status::OK;
    }

    grpc::Status Sign(
        grpc::ServerContext *context,
        const kms::SignRequest *request,
        kms::SignResponse *response
    ) override {
        (void)context;

        unsigned char signature[512];
        size_t signatureLength = 0;

        const std::string &payload = request->payload();

        uint32_t status = KmsServiceSign(
            request->client_id().c_str(),
            request->key_id().c_str(),
            reinterpret_cast<const unsigned char *>(payload.data()),
            payload.size(),
            signature,
            sizeof(signature),
            &signatureLength
        );

        if (status != KMS_OK) {
            return KmsMapStatus(status);
        }

        response->set_signature(signature, signatureLength);
        return grpc::Status::OK;
    }

    grpc::Status GetPublicKey(
        grpc::ServerContext *context,
        const kms::GetPublicKeyRequest *request,
        kms::GetPublicKeyResponse *response
    ) override {
        (void)context;

        std::vector<unsigned char> publicKey(8192);
        size_t publicKeyLength = 0;

        uint32_t status = KmsServiceGetPublicKey(
            request->client_id().c_str(),
            request->key_id().c_str(),
            publicKey.data(),
            publicKey.size(),
            &publicKeyLength
        );

        if (status != KMS_OK) {
            return KmsMapStatus(status);
        }

        response->set_public_key_pem(publicKey.data(), publicKeyLength);
        return grpc::Status::OK;
    }
};

}  // namespace

extern "C" uint32_t
KmsGrpcServerRun(
    const char *socketPath
) {
    std::string address = std::string("unix:") + socketPath;

    KmsServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        return KMS_ERR_INTERNAL;
    }

    server->Wait();
    return KMS_OK;
}

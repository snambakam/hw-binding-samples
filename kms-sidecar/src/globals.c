#include "includes.h"

int gKmsServerFd = -1;
int gKmsRateLimitPerMin = KMS_DEFAULT_RATE_LIMIT_PER_MIN;
int gKmsRateEventCount = 0;
time_t gKmsRateEvents[KMS_MAX_RATE_EVENTS] = {0};
char gKmsSocketPath[PATH_MAX] = KMS_DEFAULT_SOCKET_PATH;
char gKmsBackend[KMS_MAX_TEXT] = KMS_DEFAULT_BACKEND;
char gKmsPolicyClientId[KMS_MAX_TEXT] = KMS_DEFAULT_CLIENT_ID;
char gKmsPolicyKeyId[KMS_MAX_TEXT] = KMS_DEFAULT_KEY_ID;
char gKmsFapiKeyPath[PATH_MAX] = KMS_DEFAULT_FAPI_KEY_PATH;
char gKmsTpmPublicPem[PATH_MAX] = "/keys/server-public.pem";
EVP_PKEY *gKmsDevPrivateKey = NULL;
FAPI_CONTEXT *gKmsFapiContext = NULL;

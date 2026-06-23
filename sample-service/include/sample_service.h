#ifndef SAMPLE_SERVICE_H
#define SAMPLE_SERVICE_H

#include <stdint.h>

/*
 * Sample service error codes.
 *
 * Range: 0x00002000 - 0x00002FFF
 * A value of SAMPLE_OK (0) indicates success; any non-zero value is a failure.
 */
#define SAMPLE_OK 0x00000000U
#define SAMPLE_ERR_CONFIG 0x00002001U
#define SAMPLE_ERR_SOCKET_CONNECT 0x00002002U
#define SAMPLE_ERR_SOCKET_IO 0x00002003U
#define SAMPLE_ERR_BASE64_ENCODE 0x00002004U
#define SAMPLE_ERR_BASE64_DECODE 0x00002005U
#define SAMPLE_ERR_KMS_HEALTH 0x00002006U
#define SAMPLE_ERR_KMS_SIGN 0x00002007U
#define SAMPLE_ERR_KMS_PUBLIC_KEY 0x00002008U
#define SAMPLE_ERR_RESPONSE_FORMAT 0x00002009U
#define SAMPLE_ERR_PUBLIC_KEY_PARSE 0x0000200AU
#define SAMPLE_ERR_VERIFY 0x0000200BU
#define SAMPLE_ERR_INTERNAL 0x0000200CU

uint32_t SampleServiceInitialize(void);
uint32_t SampleServiceRun(void);
void SampleServiceFinalize(void);

#endif

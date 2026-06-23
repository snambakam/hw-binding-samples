#include "includes.h"

int
main(
    void
) {
    uint32_t status;

    status = KmsSidecarInitialize();
    if (status != KMS_OK) {
        fprintf(stderr, "kms-sidecar initialize failed (0x%08X)\n", status);
        return (int)status;
    }

    status = KmsSidecarRun();
    if (status != KMS_OK) {
        fprintf(stderr, "kms-sidecar run failed (0x%08X)\n", status);
        KmsSidecarFinalize();
        return (int)status;
    }

    KmsSidecarFinalize();
    return 0;
}

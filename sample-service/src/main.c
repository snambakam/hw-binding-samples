#include "includes.h"

int
main(
    void
) {
    uint32_t status;

    status = SampleServiceInitialize();
    if (status != SAMPLE_OK) {
        fprintf(stderr, "sample-service initialize failed (0x%08X)\n", status);
        return (int)status;
    }

    status = SampleServiceRun();
    if (status != SAMPLE_OK) {
        fprintf(stderr, "sample-service run failed (0x%08X)\n", status);
        SampleServiceFinalize();
        return (int)status;
    }

    SampleServiceFinalize();
    return 0;
}

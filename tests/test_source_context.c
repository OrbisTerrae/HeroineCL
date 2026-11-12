#include <stdint.h>
#include <stdio.h>

#include "GPMF_mp4reader.h"

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    GPMF_Source *first = GPMF_SourceCreate();
    GPMF_Source *second = GPMF_SourceCreate();
    if (!first || !second) {
        GPMF_SourceDestroy(first);
        GPMF_SourceDestroy(second);
        return 3;
    }
    int result = 1;
    uint32_t *payload = NULL;
    GPMF_stream stream = {0};

    if (OpenGPMFSource(first, argv[1]) <= 0.0 ||
        OpenGPMFSource(second, argv[1]) <= 0.0) {
        fprintf(stderr, "could not open source in independent contexts\n");
        goto cleanup;
    }
    uint32_t first_count = GetNumberGPMFPayloads(first);
    uint32_t second_count = GetNumberGPMFPayloads(second);
    if (!first_count || first_count != second_count) { result = 4; goto cleanup; }

    payload = GetGPMFPayload(first, NULL, 0);
    int has_gps = payload && GPMF_OK == GPMF_Init(&stream, payload, GetGPMFPayloadSize(first, 0)) &&
                  GPMF_OK == GPMF_FindNext(&stream, STR2FOURCC("GPS5"), GPMF_RECURSE_LEVELS);
    int independent = GetNumberGPMFPayloads(second) == second_count &&
                      GetGPMFPayloadSize(second, 0) > 0;
    result = (!has_gps || !independent) ? 1 : 0;
    if (result) fprintf(stderr, "source context state was not isolated\n");

cleanup:
    if (payload) FreeGPMFPayload(first, payload);
    if (stream.cbhandle) GPMF_Free(&stream);
    CloseGPMFSource(first);
    GPMF_SourceDestroy(first);
    GPMF_SourceDestroy(second);
    if (result == 0) puts("Independent reader contexts passed");
    return result;
}

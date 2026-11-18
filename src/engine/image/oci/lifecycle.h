#ifndef _NASH_ENGINE_IMAGE_OCI_LIFECYCLE_H
#define _NASH_ENGINE_IMAGE_OCI_LIFECYCLE_H

#include <stddef.h>
#include "common/utils/macros.h"

typedef struct {
    char *ref;
    ARRAY_WITH_LEN(char *, blob_digests);
} nsh_oci_lc_context_t;

void nsh_oci_lc_context_destroy(nsh_oci_lc_context_t *ctx);

int nsh_oci_image_import(const char *bundle_path, nsh_oci_lc_context_t *ctx);

#endif

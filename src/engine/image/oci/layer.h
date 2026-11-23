#ifndef _NASH_ENGINE_IMAGE_OCI_LAYER_H
#define _NASH_ENGINE_IMAGE_OCI_LAYER_H

#include <stdint.h>

int nsh_oci_layer_unpack(
    const char *path, const char *target,
    int format, int filter, int64_t *out_size);

int nsh_oci_layer_pack(
    const char *path, const char *source,
    int format, int filter);

#endif

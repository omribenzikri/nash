#ifndef _NASH_ENGINE_IMAGE_OCI_LIFECYCLE_H
#define _NASH_ENGINE_IMAGE_OCI_LIFECYCLE_H

#include <stdint.h>
#include <linux/limits.h>

typedef struct {
    char ref[NAME_MAX];
    int64_t size;
} nsh_oci_image_desc_t;

int nsh_oci_image_import(
    const char *store_path, const char *bundle_path,
    nsh_oci_image_desc_t *desc);

int nsh_oci_image_delete(const char *store_path, const char *ref);

#endif

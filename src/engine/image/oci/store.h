#ifndef _NASH_ENGINE_IMAGE_OCI_STORE_H
#define _NASH_ENGINE_IMAGE_OCI_STORE_H

#include <stddef.h>
#include "engine/image/oci/common.h"

int nsh_oci_store_create(const char *store_path);

void nsh_oci_store_manifest_path_resolve(
    const char *store_path, const char *digest,
    char *buf, size_t size);

void nsh_oci_store_config_path_resolve(
    const char *store_path, const char *digest,
    char *buf, size_t size);

void nsh_oci_store_snapshot_path_resolve(
    const char *store_path, const char *digest,
    char *buf, size_t size);

void nsh_oci_store_blob_path_resolve(
    const char *store_path, const char *digest,
    nsh_oci_blob_type_t type, char *buf, size_t size);

int nsh_oci_store_load_image_meta(nsh_oci_image_meta_t *meta,
    const char *store_path, const char *ref);

int nsh_oci_store_snapshot_paths(nsh_oci_image_meta_t *meta,
    const char *store_path, char ***outp, size_t *outl);

#endif

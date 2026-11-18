#ifndef _NASH_ENGINE_IMAGE_OCI_STORE_H
#define _NASH_ENGINE_IMAGE_OCI_STORE_H

#include "engine/image/oci/common.h"

typedef struct {
    const char *snapshots_repo_path;
    const char *manifests_repo_path;
    const char *config_repo_path;
} nsh_oci_image_store_t;

void nsh_oci_store_blob_path_resolve(
    const char *repo_path, const char *digest,
    char *buf, size_t size);

int nsh_oci_store_load_image_meta(nsh_oci_image_meta_t *meta,
    nsh_oci_image_store_t *store, const char *ref);

#endif

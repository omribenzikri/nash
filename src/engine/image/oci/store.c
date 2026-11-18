#include <linux/limits.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>
#include "engine/image/oci/common.h"
#include "engine/image/oci/parse.h"
#include "engine/image/oci/store.h"

inline void nsh_oci_store_blob_path_resolve(
    const char *repo_path, const char *digest,
    char *buf, size_t size)
{
    snprintf(buf, size, "%s/%s", repo_path, digest);
}

static image_spec_schema_image_manifest_schema *nsh_oci_store_manifest_load(
    nsh_oci_image_store_t *store, const char *digest)
{
    char path[PATH_MAX];

    nsh_oci_store_blob_path_resolve(
        store->manifests_repo_path, digest,
        path, sizeof(path));

    return nsh_oci_manifest_parse(path);
}

static image_spec_schema_config_schema *nsh_oci_store_config_load(
    nsh_oci_image_store_t *store, const char *digest)
{
    char path[PATH_MAX];

    nsh_oci_store_blob_path_resolve(
        store->config_repo_path, digest,
        path, sizeof(path));

    return nsh_oci_config_parse(path);
}

int nsh_oci_store_load_image_meta(
    nsh_oci_image_meta_t *meta, nsh_oci_image_store_t *store, const char *ref)
{
    meta->manifest = nsh_oci_store_manifest_load(store, ref);
    if (meta->manifest == NULL)
        goto error;

    meta->config = nsh_oci_store_config_load(store, meta->manifest->config->digest);
    if (meta->config == NULL)
        goto error_free_manifest;

    return 0;

error_free_manifest:
    free_image_spec_schema_image_manifest_schema(meta->manifest);
error:
    return -1;
}

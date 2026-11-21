#include <assert.h>
#include <linux/limits.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>
#include <stdlib.h>
#include <string.h>
#include "common/utils/filesystem.h"
#include "common/utils/macros.h"
#include "engine/image/oci/common.h"
#include "engine/image/oci/parse.h"
#include "engine/image/oci/store.h"

#define NSH_OCI_STORE_MANIFESTS_REPO "manifests"
#define NSH_OCI_STORE_CONFIG_REPO "config"
#define NSH_OCI_STORE_SNAPSHOTS_REPO "snapshots"

int nsh_oci_store_create(const char *store_path)
{
    char path[PATH_MAX];
    int err;

    snprintf(path, sizeof(path), "%s/%s", store_path, NSH_OCI_STORE_MANIFESTS_REPO);
    err = mkdir_if_missing(path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create repository: " NSH_OCI_STORE_MANIFESTS_REPO);
        return -1;
    }

    snprintf(path, sizeof(path), "%s/%s", store_path, NSH_OCI_STORE_CONFIG_REPO);
    err = mkdir_if_missing(path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create repository: " NSH_OCI_STORE_CONFIG_REPO);
        return -1;
    }

    snprintf(path, sizeof(path), "%s/%s", store_path, NSH_OCI_STORE_SNAPSHOTS_REPO);
    err = mkdir_if_missing(path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create repository: " NSH_OCI_STORE_SNAPSHOTS_REPO);
        return -1;
    }

    return 0;
}

inline void nsh_oci_store_manifest_path_resolve(
    const char *store_path, const char *digest, char *buf, size_t size)
{
    snprintf(buf, size, "%s/%s/%s",
        store_path, NSH_OCI_STORE_MANIFESTS_REPO, digest);
}

inline void nsh_oci_store_config_path_resolve(
    const char *store_path, const char *digest, char *buf, size_t size)
{
    snprintf(buf, size, "%s/%s/%s",
        store_path, NSH_OCI_STORE_CONFIG_REPO, digest);
}

inline void nsh_oci_store_snapshot_path_resolve(
    const char *store_path, const char *digest, char *buf, size_t size)
{
    snprintf(buf, size, "%s/%s/%s",
        store_path, NSH_OCI_STORE_SNAPSHOTS_REPO, digest);
}

static image_spec_schema_image_manifest_schema *nsh_oci_store_manifest_load(
    const char *store_path, const char *digest)
{
    char path[PATH_MAX];

    nsh_oci_store_manifest_path_resolve(
        store_path, digest, path, sizeof(path));

    return nsh_oci_manifest_parse(path);
}

static image_spec_schema_config_schema *nsh_oci_store_config_load(
    const char *store_path, const char *digest)
{
    char path[PATH_MAX];

    nsh_oci_store_config_path_resolve(
        store_path, digest, path, sizeof(path));

    return nsh_oci_config_parse(path);
}

int nsh_oci_store_load_image_meta(
    nsh_oci_image_meta_t *meta, const char *store_path, const char *ref)
{
    meta->manifest = nsh_oci_store_manifest_load(store_path, ref);
    if (meta->manifest == NULL)
        goto error;

    meta->config = nsh_oci_store_config_load(
        store_path, meta->manifest->config->digest);
    if (meta->config == NULL)
        goto error_free_manifest;

    return 0;

error_free_manifest:
    free_image_spec_schema_image_manifest_schema(meta->manifest);
error:
    return -1;
}

int nsh_oci_store_snapshot_paths(
    nsh_oci_image_meta_t *meta, const char *store_path, char ***outp, size_t *outl)
{
    ARRAY_WITH_LEN(char *, paths);
    size_t nlayers = meta->manifest->layers_len;
    char path[PATH_MAX];

    paths_len = 0;
    paths = calloc(nlayers, sizeof(*paths));
    if (paths == NULL) {
        perror("[x] Failed to allocate snapshots paths array");
        goto error;
    }

    for (size_t i = 0; i < nlayers; i++) {
        nsh_oci_store_snapshot_path_resolve(
            store_path, meta->manifest->layers[i]->digest,
            path, sizeof(path));

        paths[i] = strndup(path, sizeof(path));
        if (paths[i] == NULL) {
            perror("[x] Failed to allocate snapshot path");
            goto error_free_paths;
        }
        paths_len++;
    }

    assert(paths_len == nlayers);

    *outp = paths;
    *outl = paths_len;
    return 0;

error_free_paths:
    while (paths_len > 0)
        free(paths[--paths_len]);
    free(paths);
error:
    return -1;
}

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <ocispec/image_spec_schema_image_layout_schema.h>
#include <ocispec/image_spec_schema_image_index_schema.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>
#include "common/utils/filesystem.h"
#include "engine/image/oci/common.h"
#include "engine/image/oci/parse.h"
#include "engine/image/oci/bundle.h"

static inline const char *nsh_oci_get_arch()
{
#if defined(__x86_64__) || defined(_M_X64)
    return "amd64";
#elif defined(__i386__) || defined(_M_IX86)
    return "386";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#elif defined(__riscv) && __riscv_xlen == 64
    return "riscv64";
#else
    // No support for additional architectures
    return NULL;
#endif
}

static inline const char *nsh_oci_get_os()
{
#if defined(__linux__)
    return "linux";
#else
    // No support for additional systems
    return NULL;
#endif
}

static bool nsh_oci_is_compat_platform(
    image_spec_schema_image_index_schema_manifests_platform *platform)
{
    const char *arch = nsh_oci_get_arch();
    const char *os = nsh_oci_get_os();

    if (arch == NULL || os == NULL)
        return false;

    return strcmp(platform->architecture, arch) == 0 &&
           strcmp(platform->os, os) == 0;
}

static image_spec_schema_image_index_schema_manifests_element
    *nsh_oci_find_compat_manifest_element(
        image_spec_schema_image_index_schema *index)
{
    image_spec_schema_image_index_schema_manifests_element *element;

    for (size_t i = 0; i < index->manifests_len; i++) {
        element = index->manifests[i];

        // If no platform is specified, assume compatibility...
        if (element->platform != NULL &&
            !nsh_oci_is_compat_platform(element->platform)) continue;

        return element;
    }

    return NULL;
}

static inline void nsh_oci_bundle_layout_path_resolve(
    const char *bundle_path, char *buf, size_t size)
{
    snprintf(buf, size, "%s/oci-layout", bundle_path);
}

static inline void nsh_oci_bundle_index_path_resolve(
    const char *bundle_path, char *buf, size_t size)
{
    snprintf(buf, size, "%s/index.json", bundle_path);
}

static inline void nsh_oci_bundle_blobs_dir_path_resolve(
    const char *bundle_path, char *buf, size_t size)
{
    snprintf(buf, size, "%s/blobs", bundle_path);
}

int nsh_oci_bundle_blob_path_resolve(
    const char *bundle_path, const char *digest,
    char *buf, size_t size)
{
    char base_path[PATH_MAX];
    char alg[NAME_MAX + 1];
    char encoded[NAME_MAX + 1];
    int err;

    nsh_oci_bundle_blobs_dir_path_resolve(bundle_path, base_path, sizeof(base_path));

    err = nsh_oci_digest_parse(digest, alg, sizeof(alg), encoded, sizeof(encoded));
    if (err)
        return -1;

    snprintf(buf, size, "%s/%s/%s", base_path, alg, encoded);

    return 0;
}

static image_spec_schema_image_layout_schema *nsh_oci_bundle_layout_load(const char *bundle_path)
{
    char path[PATH_MAX];

    nsh_oci_bundle_layout_path_resolve(bundle_path, path, sizeof(path));

    return nsh_oci_layout_parse(path);
}

static image_spec_schema_image_index_schema *nsh_oci_bundle_index_load(const char *bundle_path)
{
    char path[PATH_MAX];

    nsh_oci_bundle_index_path_resolve(bundle_path, path, sizeof(path));

    return nsh_oci_index_parse(path);
}

static image_spec_schema_image_manifest_schema *nsh_oci_bundle_manifest_load(
    const char *bundle_path, const char *digest)
{
    char path[PATH_MAX];
    int err;

    err = nsh_oci_bundle_blob_path_resolve(
        bundle_path, digest, path, sizeof(path));
    if (err)
        return NULL;

    return nsh_oci_manifest_parse(path);
}

static image_spec_schema_config_schema *nsh_oci_bundle_config_load(
    const char *bundle_path, const char *digest)
{
    char path[PATH_MAX];
    int err;

    err = nsh_oci_bundle_blob_path_resolve(
        bundle_path, digest, path, sizeof(path));
    if (err)
        return NULL;

    return nsh_oci_config_parse(path);
}

static int nsh_oci_bundle_load_image_meta_internal(nsh_oci_image_meta_t *meta,
    const char *bundle_path, image_spec_schema_image_index_schema *index,
    char *outb, size_t outl)
{
    image_spec_schema_image_index_schema_manifests_element *manifest_element;

    manifest_element = nsh_oci_find_compat_manifest_element(index);
    if (manifest_element == NULL) {
        fprintf(stderr, "[x] Failed to find a compatible manifest in index\n");
        goto error;
    }

    meta->manifest = nsh_oci_bundle_manifest_load(
        bundle_path, manifest_element->digest);
    if (meta->manifest == NULL)
        goto error;

    meta->config = nsh_oci_bundle_config_load(
        bundle_path, meta->manifest->config->digest);
    if (meta->config == NULL)
        goto error_free_manifest;

    snprintf(outb, outl, "%s", manifest_element->digest);

    return 0;

error_free_manifest:
    free_image_spec_schema_image_manifest_schema(meta->manifest);
error:
    return -1;
}

int nsh_oci_bundle_load_image_meta(nsh_oci_image_meta_t *meta,
    const char *bundle_path, char *outb, size_t outl)
{
    image_spec_schema_image_layout_schema *layout;
    image_spec_schema_image_index_schema *index;
    int err;
    int ret = 0;

    layout = nsh_oci_bundle_layout_load(bundle_path);
    if (layout == NULL) {
        ret = -1;
        goto out;
    }

    index = nsh_oci_bundle_index_load(bundle_path);
    if (index == NULL) {
        ret = -1;
        goto out_free_layout;
    }

    err = nsh_oci_bundle_load_image_meta_internal(
        meta, bundle_path, index, outb, outl);
    if (err) {
        ret = -1;
        goto out_free_index;
    }

out_free_index:
    free_image_spec_schema_image_index_schema(index);
out_free_layout:
    free_image_spec_schema_image_layout_schema(layout);
out:
    return ret;
}

static int nsh_oci_bundle_layout_dump(const char *bundle_path)
{
    char path[PATH_MAX];

    nsh_oci_bundle_layout_path_resolve(bundle_path, path, sizeof(path));

    return nsh_oci_layout_dump(path);
}

static int nsh_oci_bundle_index_dump(const char *bundle_path,
    const char *manifest_digest, size_t manifest_size)
{
    char path[PATH_MAX];

    nsh_oci_bundle_index_path_resolve(bundle_path, path, sizeof(path));

    return nsh_oci_index_dump(path, manifest_digest, manifest_size);
}

int nsh_oci_bundle_create(const char *bundle_path)
{
    char blobs_path[PATH_MAX];
    int err;

    // TEMP
    rmdir_recursive(bundle_path);

    nsh_oci_bundle_blobs_dir_path_resolve(
        bundle_path, blobs_path, sizeof(blobs_path));

    err = mkdir(bundle_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (err) {
        perror("[x] Failed to create target bundle directory");
        return -1;
    }

    err = mkdir(blobs_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (err) {
        perror("[x] Failed to create target blobs directory");
        return -1;
    }

    return 0;
}

int nsh_oci_bundle_seal(const char *bundle_path, const char *ref)
{
    const char *manifest_digest;
    size_t manifest_size;
    int err;

    manifest_digest = ref;
    manifest_size = 0; // TODO

    err = nsh_oci_bundle_index_dump(bundle_path, manifest_digest, manifest_size);
    if (err)
        return -1;

    err = nsh_oci_bundle_layout_dump(bundle_path);
    if (err)
        return -1;

    return 0;
}

int nsh_oci_bundle_dump_image_meta(
    nsh_oci_image_meta_t *meta, const char *bundle_path)
{
    (void)bundle_path;
    (void)meta;

    return 0;
}

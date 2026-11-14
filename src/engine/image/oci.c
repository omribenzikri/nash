#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <linux/limits.h>
#include <ocispec/json_common.h>
#include <ocispec/image_spec_schema_image_layout_schema.h>
#include <ocispec/image_spec_schema_image_index_schema.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_content_descriptor.h>
#include <ocispec/image_spec_schema_config_schema.h>
#include <archive.h>
#include "common/utils/filesystem.h"
#include "common/utils/string.h"
#include "common/utils/hashing.h"
#include "engine/image/oci/unpack.h"
#include "engine/image/oci.h"

#define OCI_WHITEOUT_PREFIX ".wh."
#define OCI_OPAQUE_WHITEOUT_NAME ".wh..wh..opq"

#define OCI_HASH_ALG_SHA256 "sha256"
#define OCI_HASH_ALG_SHA512 "sha512"
#define OCI_HASH_ALG_BLAKE3 "blake3"

#define NSH_OCI_SUPPORTED_IMAGE_LAYOUT_VERSION "1.0.0"

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
    *nsh_oci_find_compat_image_manifest(image_spec_schema_image_index_schema *index)
{
    image_spec_schema_image_index_schema_manifests_element *element;

    for (size_t i = 0; i < index->manifests_len; i++) {
        element = index->manifests[i];

        if (strcmp(element->media_type, OCI_MEDIA_TYPE_MANIFEST)!= 0)
            continue;

        // If no platform is specified, assume compatibility...
        if (element->platform != NULL &&
            !nsh_oci_is_compat_platform(element->platform)) continue;

        return element;
    }

    return NULL;
}

static int nsh_oci_digest_parse(const char *digest,
    char *alg_buf, size_t alg_size, char *encoded_buf, size_t encoded_size)
{
    char copy[NAME_MAX + 1];
    char *alg;
    char *encoded;

    snprintf(copy, sizeof(copy), "%s", digest);

    alg = strtok(copy, ":");
    if (alg == NULL)
        return -1;

    encoded = strtok(NULL, ":");
    if (encoded == NULL)
        return -1;

    if (strtok(NULL, ":") != NULL)
        return -1;

    snprintf(alg_buf, alg_size, "%s", alg);
    snprintf(encoded_buf, encoded_size, "%s", encoded);

    return 0;
}

static void nsh_oci_digest_components_to_path(
    const char *image_path, const char *alg, const char *encoded, char *buf, size_t size)
{
    snprintf(buf, size, "%s/blobs/%s/%s", image_path, alg, encoded);
}

int nsh_oci_digest_to_path(const char *image_path, const char *digest, char *buf, size_t size)
{
    char alg[NAME_MAX + 1];
    char encoded[NAME_MAX + 1];
    int err;

    err = nsh_oci_digest_parse(digest, alg, sizeof(alg), encoded, sizeof(encoded));
    if (err) {
        fprintf(stderr, "[x] Failed to parse blob digest: %s\n", digest);
        return -1;
    }

    nsh_oci_digest_components_to_path(image_path, alg, encoded, buf, size);

    return 0;
}

static image_spec_schema_image_layout_schema *nsh_oci_load_image_layout(const char *image_path)
{
    char layout_file_path[PATH_MAX];
    image_spec_schema_image_layout_schema *layout;
    parser_error err;

    snprintf(layout_file_path, sizeof(layout_file_path), "%s/oci-layout", image_path);

    layout = image_spec_schema_image_layout_schema_parse_file(layout_file_path, NULL, &err);
    if (layout == NULL) {
        fprintf(stderr, "[x] Failed to parse image layout: %s\n", err);
        goto error;
    }

    if (strcmp(layout->image_layout_version, NSH_OCI_SUPPORTED_IMAGE_LAYOUT_VERSION) != 0) {
        fprintf(stderr, "[x] Image contains an unsupported layout version\n");
        goto error_free_layout;
    }

    return layout;

error_free_layout:
    free_image_spec_schema_image_layout_schema(layout);
error:
    return NULL;
}

static image_spec_schema_image_index_schema *nsh_oci_load_image_index(const char *image_path)
{
    char index_file_path[PATH_MAX];
    image_spec_schema_image_index_schema *index;
    parser_error err;

    snprintf(index_file_path, sizeof(index_file_path), "%s/index.json", image_path);

    index = image_spec_schema_image_index_schema_parse_file(index_file_path, NULL, &err);
    if (index == NULL) {
        fprintf(stderr, "[x] Failed to parse image index: %s\n", err);
        return NULL;
    }

    return index;
}

static image_spec_schema_image_manifest_schema *nsh_oci_load_image_manifest(
    const char *image_path, const char *digest)
{
    char manifest_file_path[PATH_MAX];
    image_spec_schema_image_manifest_schema *manifest;
    parser_error parser_err;
    int err;

    err = nsh_oci_digest_to_path(image_path, digest,
        manifest_file_path, sizeof(manifest_file_path));
    if (err) {
        fprintf(stderr, "[x] Failed to resolve manifest blob path by digest\n");
        goto error;
    }

    manifest = image_spec_schema_image_manifest_schema_parse_file(manifest_file_path, NULL, &parser_err);
    if (manifest == NULL) {
        fprintf(stderr, "[x] Failed to parse image manifest: %s\n", parser_err);
        goto error;
    }

    if (strcmp(manifest->config->media_type, OCI_MEDIA_TYPE_CONFIG)!= 0) {
        fprintf(stderr, "[x] Image config reference does not contain a valid media type\n");
        goto error_free_manifest;
    }

    return manifest;

error_free_manifest:
    free_image_spec_schema_image_manifest_schema(manifest);
error:
    return NULL;
}

static image_spec_schema_config_schema *nsh_oci_load_image_config(
    const char *image_path, const char *digest)
{
    char config_file_path[PATH_MAX];
    image_spec_schema_config_schema *config;
    parser_error parser_err;
    int err;

    err = nsh_oci_digest_to_path(image_path, digest,
        config_file_path, sizeof(config_file_path));
    if (err) {
        fprintf(stderr, "[x] Failed to resolve config blob path by digest\n");
        return NULL;
    }

    config = image_spec_schema_config_schema_parse_file(config_file_path, NULL, &parser_err);
    if (config == NULL) {
        fprintf(stderr, "[x] Failed to parse image config: %s\n", parser_err);
        return NULL;
    }

    return config;
}

int nsh_oci_image_load(oci_image_t *image, const char *path)
{
    image_spec_schema_image_index_schema_manifests_element *manifest_element;

    image->layout = nsh_oci_load_image_layout(path);
    if (image->layout == NULL)
        goto error;

    image->index = nsh_oci_load_image_index(path);
    if (image->index == NULL)
        goto error_free_layout;

    manifest_element = nsh_oci_find_compat_image_manifest(image->index);
    if (manifest_element == NULL) {
        fprintf(stderr, "[x] Failed to find a compatible image manifest in the index\n");
        goto error_free_index;
    }

    image->manifest = nsh_oci_load_image_manifest(path, manifest_element->digest);
    if (image->manifest == NULL)
        goto error_free_index;

    image->config = nsh_oci_load_image_config(path, image->manifest->config->digest);
    if (image->config == NULL)
        goto error_free_manifest;

    return 0;

error_free_manifest:
    free_image_spec_schema_image_manifest_schema(image->manifest);
error_free_index:
    free_image_spec_schema_image_index_schema(image->index);
error_free_layout:
    free_image_spec_schema_image_layout_schema(image->layout);
error:
    return -1;
}

void nsh_oci_image_free(oci_image_t *image)
{
    free_image_spec_schema_image_layout_schema(image->layout);
    free_image_spec_schema_image_index_schema(image->index);
    free_image_spec_schema_image_manifest_schema(image->manifest);
    free_image_spec_schema_config_schema(image->config);
}

static int nsh_oci_image_meta_load_internal(nsh_oci_image_meta_t *meta,
    const char *path, image_spec_schema_image_index_schema *index)
{
    image_spec_schema_image_index_schema_manifests_element *manifest_element;

    manifest_element = nsh_oci_find_compat_image_manifest(index);
    if (manifest_element == NULL) {
        fprintf(stderr,
            "[x] Cannot a compatible manifest in image index\n");
        goto error;
    }

    meta->ref = strndup(manifest_element->digest, NAME_MAX);
    if (meta->ref == NULL) {
        perror("[x] Failed to allocate manifest digest");
        goto error;
    }

    meta->manifest = nsh_oci_load_image_manifest(path, meta->ref);
    if (meta->manifest == NULL)
        goto error_free_ref;

    meta->config = nsh_oci_load_image_config(path, meta->manifest->config->digest);
    if (meta->config == NULL)
        goto error_free_manifest;

    return 0;

error_free_manifest:
    free_image_spec_schema_image_manifest_schema(meta->manifest);
error_free_ref:
    free(meta->ref);
error:
    return -1;
}

int nsh_oci_image_meta_load(nsh_oci_image_meta_t *meta, const char *path)
{
    image_spec_schema_image_layout_schema *layout;
    image_spec_schema_image_index_schema *index;
    int err;
    int ret = 0;

    layout = nsh_oci_load_image_layout(path);
    if (layout == NULL) {
        ret = -1;
        goto out;
    }

    index = nsh_oci_load_image_index(path);
    if (index == NULL) {
        ret = -1;
        goto out_free_layout;
    }

    err = nsh_oci_image_meta_load_internal(meta, path, index);
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

void nsh_oci_image_meta_destroy(nsh_oci_image_meta_t *meta)
{
    free(meta->ref);
    free_image_spec_schema_image_manifest_schema(meta->manifest);
    free_image_spec_schema_config_schema(meta->config);
}

bool nsh_oci_is_whiteout(const char *pathname)
{
    char base_name[PATH_MAX];

    split_path(pathname, NULL, 0, base_name, sizeof(base_name));

    return strncmp(base_name, OCI_WHITEOUT_PREFIX,
        strlen(OCI_WHITEOUT_PREFIX)) == 0;
}

bool nsh_oci_is_valid_whiteout(const char *pathname)
{
    char base_name[PATH_MAX];

    split_path(pathname, NULL, 0, base_name, sizeof(base_name));

    return strcmp(base_name, OCI_WHITEOUT_PREFIX) != 0;
}

bool nsh_oci_is_opaque_whiteout(const char *pathname)
{
    char base_name[PATH_MAX];

    split_path(pathname, NULL, 0, base_name, sizeof(base_name));

    return strcmp(base_name, OCI_OPAQUE_WHITEOUT_NAME) == 0;
}

bool nsh_oci_is_regular_whiteout(const char *pathname)
{
    return nsh_oci_is_whiteout(pathname) &&
           !nsh_oci_is_opaque_whiteout(pathname);
}

int nsh_oci_whiteout_to_file(const char *pathname, char *buf, size_t size)
{
    char dir_name[PATH_MAX];
    char base_name[PATH_MAX];

    split_path(pathname,
        dir_name, sizeof(dir_name),
        base_name, sizeof(base_name));

    if (strlen(base_name) <= strlen(OCI_WHITEOUT_PREFIX))
        return -1;

    snprintf(buf, size, "%s/%s", dir_name, base_name + strlen(OCI_WHITEOUT_PREFIX));

    return 0;
}

static int nsh_oci_verify_blob_hash(
    const char *path, const char *alg, const char *encoded, bool *out)
{
    union {
        uint8_t sha256[crypto_hash_sha256_BYTES];
        uint8_t sha512[crypto_hash_sha512_BYTES];
        uint8_t blake3[BLAKE3_OUT_LEN];
    } expected;
    ssize_t expected_len;

    expected_len = str_parse_hex(encoded, (uint8_t *)&expected, sizeof(expected));
    if (expected_len == -1) {
        fprintf(stderr, "[x] Failed to parse digest encoded: %s\n", encoded);
        return -1;
    }

    if (strcmp(alg, OCI_HASH_ALG_SHA256) == 0) {
        if (expected_len != sizeof(expected.sha256)) {
            *out = false;
            return 0;
        }
        return sha256_verify_file(path, expected.sha256, out);
    }
    else if (strcmp(alg, OCI_HASH_ALG_SHA512) == 0) {
        if (expected_len != sizeof(expected.sha512)) {
            *out = false;
            return 0;
        }
        return sha512_verify_file(path, expected.sha512, out);
    }
    else if (strcmp(alg, OCI_HASH_ALG_BLAKE3) == 0) {
        if (expected_len != sizeof(expected.blake3)) {
            *out = false;
            return 0;
        }
        return blake3_verify_file(path, expected.blake3, out);
    }
    else {
        fprintf(stderr, "[x] Unsupported hash algorithm: %s\n", alg);
        return -1;
    }
}

static int nsh_oci_verify_blob(const char *path, const char *digest, bool *out)
{
    char alg[NAME_MAX + 1];
    char encoded[NAME_MAX + 1];
    int err;

    err = nsh_oci_digest_parse(digest, alg, sizeof(alg), encoded, sizeof(encoded));
    if (err) {
        fprintf(stderr, "[x] Failed to parse blob digest: %s\n", digest);
        return -1;
    }

    err = nsh_oci_verify_blob_hash(path, alg, encoded, out);
    if (err)
        return -1;

    return 0;
}

static int nsh_oci_import_blob(
    const char *repo_path, const char *image_path, const char *digest)
{
    char blob_path[PATH_MAX];
    char dest_path[PATH_MAX];
    bool is_verified;
    int err;

    err = nsh_oci_digest_to_path(image_path, digest, blob_path, sizeof(blob_path));
    if (err)
        return -1;

    snprintf(dest_path, sizeof(dest_path), "%s/%s", repo_path, digest);

    err = nsh_oci_verify_blob(blob_path, digest, &is_verified);
    if (err || !is_verified) {
        fprintf(stderr, "[x] Failed to verify blob with digest: %s\n", digest);
        return -1;
    }

    if (access(dest_path, F_OK) == 0) {
        printf("[*] Blob: %s already exists, skipping...\n", digest);
        return 0;
    }

    err = copy_file(blob_path, dest_path);
    if (err) {
        perror("[x] Failed to copy blob");
        return -1;
    }

    return 0;
}

int nsh_oci_import_manifest(const char *repo_path,
    const char *image_path, nsh_oci_image_meta_t *meta)
{
    return nsh_oci_import_blob(repo_path, image_path, meta->ref);
}

 int nsh_oci_import_config(const char *repo_path,
     const char *image_path, nsh_oci_image_meta_t *meta)
{
    return nsh_oci_import_blob(repo_path, image_path, meta->manifest->config->digest);
}

static int nsh_oci_layer_archive_format(
    image_spec_schema_content_descriptor *descriptor)
{
    if (strncmp(descriptor->media_type, OCI_MEDIA_TYPE_TAR,
        strlen(OCI_MEDIA_TYPE_TAR)) == 0) {
            return ARCHIVE_FORMAT_TAR;
        }

    return -1;
}

static int nsh_oci_layer_archive_filter(
    image_spec_schema_content_descriptor *descriptor)
{
    if (strcmp(descriptor->media_type, OCI_MEDIA_TYPE_TAR) == 0)
        return ARCHIVE_FILTER_NONE;
    if (strcmp(descriptor->media_type, OCI_MEDIA_TYPE_TAR_GZIP) == 0)
        return ARCHIVE_FILTER_GZIP;
    if (strcmp(descriptor->media_type, OCI_MEDIA_TYPE_TAR_ZSTD) == 0)
        return ARCHIVE_FILTER_ZSTD;

    return -1;
}

static int nsh_oci_import_layer_unpack(
    const char *path, const char *dest_path,
    image_spec_schema_content_descriptor *layer_descriptor)
{
    int layer_format = nsh_oci_layer_archive_format(layer_descriptor);
    int layer_filter = nsh_oci_layer_archive_filter(layer_descriptor);
    int err;

    if (layer_format == -1 || layer_filter == -1) {
        fprintf(stderr, "[x] Unsupported layer media type: %s\n",
            layer_descriptor->media_type);
        return -1;
    }

    err = nsh_oci_layer_unpack(path, dest_path, layer_format, layer_filter);
    if (err)
        return -1;

    return 0;
}

static int nsh_oci_import_layer_single(
    const char *repo_path, const char *image_path,
    image_spec_schema_content_descriptor *layer_descriptor)
{
    char *digest = layer_descriptor->digest;
    char layer_path[PATH_MAX];
    char dest_path[PATH_MAX];
    bool is_verified;
    int err;

    err = nsh_oci_digest_to_path(image_path, digest, layer_path, sizeof(layer_path));
    if (err)
        return -1;

    snprintf(dest_path, sizeof(dest_path), "%s/%s", repo_path, digest);

    err = nsh_oci_verify_blob(layer_path, digest, &is_verified);
    if (err || !is_verified) {
        fprintf(stderr,
            "[x] Failed to verify layer with digest: %s\n", digest);
        return -1;
    }

    // TEMP
    rmdir_recursive(dest_path);

    if (access(dest_path, F_OK) == 0) {
        printf("[*] Layer: %s already exists, skipping...\n", digest);
        return 0;
    }

    err = nsh_oci_import_layer_unpack(layer_path, dest_path, layer_descriptor);
    if (err) {
        return -1;
    }

    return 0;
}

int nsh_oci_import_layers(const char *repo_path,
    const char *image_path, nsh_oci_image_meta_t *meta)
{
    int err;

    for (size_t i = 0; i < meta->manifest->layers_len; i++) {
        err = nsh_oci_import_layer_single(repo_path, image_path, meta->manifest->layers[i]);
        if (err)
            return -1;
    }

    return 0;
}

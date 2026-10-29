#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <linux/limits.h>
#include <ocispec/json_common.h>
#include <ocispec/image_spec_schema_image_layout_schema.h>
#include <ocispec/image_spec_schema_image_index_schema.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_content_descriptor.h>
#include <ocispec/image_spec_schema_config_schema.h>
#include "images/oci.h"

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

bool nsh_oci_is_compat_platform(
    image_spec_schema_image_index_schema_manifests_platform *platform)
{
    const char *arch = nsh_oci_get_arch();
    const char *os = nsh_oci_get_os();

    if (arch == NULL || os == NULL)
        return false;

    return strcmp(platform->architecture, arch) == 0 &&
           strcmp(platform->os, os) == 0;
}

image_spec_schema_image_index_schema_manifests_element *nsh_oci_find_compat_manifest(
    image_spec_schema_image_index_schema *index)
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

int nsh_oci_digest_to_path(const char *image_path, const char *digest, char *buf, size_t size)
{
    char digest_mut[PATH_MAX];
    char *alg;
    char *encoded;

    snprintf(digest_mut, sizeof(digest_mut), "%s", digest);

    alg = strtok(digest_mut, ":");
    if (alg == NULL)
        return -1;

    encoded = strtok(NULL, ":");
    if (encoded == NULL)
        return -1;

    if (strtok(NULL, ":") != NULL)
        return -1;

    snprintf(buf, size, "%s/blobs/%s/%s", image_path, alg, encoded);

    return 0;
}

image_spec_schema_image_layout_schema *nsh_oci_load_image_layout(const char *image_path)
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

image_spec_schema_image_index_schema *nsh_oci_load_image_index(const char *image_path)
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

image_spec_schema_image_manifest_schema *nsh_oci_load_image_manifest(
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

image_spec_schema_config_schema *nsh_oci_load_image_config(
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

int nsh_oci_load_image(oci_image_t *image, const char *path)
{
    image_spec_schema_image_index_schema_manifests_element *manifest_element;

    image->layout = nsh_oci_load_image_layout(path);
    if (image->layout == NULL)
        goto error;

    image->index = nsh_oci_load_image_index(path);
    if (image->index == NULL)
        goto error_free_layout;

    manifest_element = nsh_oci_find_compat_manifest(image->index);
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

void nsh_oci_free_image(oci_image_t *image)
{
    free_image_spec_schema_image_layout_schema(image->layout);
    free_image_spec_schema_image_index_schema(image->index);
    free_image_spec_schema_image_manifest_schema(image->manifest);
    free_image_spec_schema_config_schema(image->config);
}

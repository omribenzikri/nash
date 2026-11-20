#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <linux/limits.h>
#include <archive.h>
#include <ocispec/image_spec_schema_image_layout_schema.h>
#include <ocispec/image_spec_schema_image_index_schema.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>
#include "engine/image/oci/common.h"
#include "engine/image/oci/parse.h"

#define NSH_OCI_SUPPORTED_IMAGE_LAYOUT_VERSION "1.0.0"

int nsh_oci_digest_parse(const char *digest,
    char *alg_buf, size_t alg_size,
    char *encoded_buf, size_t encoded_size)
{
    char copy[NAME_MAX + 1];
    char *alg;
    char *encoded;

    snprintf(copy, sizeof(copy), "%s", digest);

    alg = strtok(copy, ":");
    if (alg == NULL) {
        fprintf(stderr, "[x] Failed to parse digest: %s: alg missing\n", digest);
        return -1;
    }

    encoded = strtok(NULL, ":");
    if (encoded == NULL) {
        fprintf(stderr, "[x] Failed to parse digest: %s: encoded missing\n", digest);
        return -1;
    }

    if (alg_buf != NULL)
        snprintf(alg_buf, alg_size, "%s", alg);

    if (encoded_buf != NULL)
        snprintf(encoded_buf, encoded_size, "%s", encoded);

    return 0;
}

image_spec_schema_image_layout_schema *
    nsh_oci_layout_parse(const char *path)
{
    image_spec_schema_image_layout_schema *layout;
    parser_error err;

    layout = image_spec_schema_image_layout_schema_parse_file(path, NULL, &err);
    if (layout == NULL) {
        fprintf(stderr, "[x] Failed to parse image layout: %s\n", err);
        goto error;
    }

    if (strcmp(layout->image_layout_version,
            NSH_OCI_SUPPORTED_IMAGE_LAYOUT_VERSION) != 0) {
            fprintf(stderr,
                "[x] Unsupported image layout version: %s\n",
                layout->image_layout_version);
            goto error_free_layout;
        }

    return layout;

error_free_layout:
    free_image_spec_schema_image_layout_schema(layout);
error:
    return NULL;
}

image_spec_schema_image_index_schema *nsh_oci_index_parse(const char *path)
{
    image_spec_schema_image_index_schema *index;
    parser_error err;

    index = image_spec_schema_image_index_schema_parse_file(path, NULL, &err);
    if (index == NULL) {
        fprintf(stderr, "[x] Failed to parse image index: %s\n", err);
        goto error;
    }

    for (size_t i = 0; i < index->manifests_len; i++) {
        if (strcmp(index->manifests[i]->media_type, OCI_MEDIA_TYPE_MANIFEST) != 0) {
            fprintf(stderr,
                "[x] Invalid manifest media type: %s\n",
                index->manifests[i]->media_type);
            goto error_free_index;
        }
    }

    return index;

error_free_index:
    free_image_spec_schema_image_index_schema(index);
error:
    return NULL;
}

image_spec_schema_image_manifest_schema *
    nsh_oci_manifest_parse(const char *path)
{
    image_spec_schema_image_manifest_schema *manifest;
    parser_error err;

    manifest = image_spec_schema_image_manifest_schema_parse_file(path, NULL, &err);
    if (manifest == NULL) {
        fprintf(stderr, "[x] Failed to parse image manifest: %s\n", err);
        goto error;
    }

    if (strcmp(manifest->config->media_type, OCI_MEDIA_TYPE_CONFIG) != 0) {
        fprintf(stderr,
            "[x] Invalid config media type: %s\n",
            manifest->config->media_type);
        goto error_free_manifest;
    }

    return manifest;

error_free_manifest:
    free_image_spec_schema_image_manifest_schema(manifest);
error:
    return NULL;
}

image_spec_schema_config_schema *nsh_oci_config_parse(const char *path)
{
    image_spec_schema_config_schema *config;
    parser_error err;

    config = image_spec_schema_config_schema_parse_file(path, NULL, &err);
    if (config == NULL) {
        fprintf(stderr, "[x] Failed to parse image config: %s\n", err);
        return NULL;
    }

    return config;
}

int nsh_oci_layer_archive_format(const char *media_type)
{
    if (strncmp(media_type, OCI_MEDIA_TYPE_TAR,
        strlen(OCI_MEDIA_TYPE_TAR)) == 0) {
            return ARCHIVE_FORMAT_TAR;
        }

    return -1;
}

int nsh_oci_layer_archive_filter(const char *media_type)
{
    if (strcmp(media_type, OCI_MEDIA_TYPE_TAR) == 0)
        return ARCHIVE_FILTER_NONE;
    if (strcmp(media_type, OCI_MEDIA_TYPE_TAR_GZIP) == 0)
        return ARCHIVE_FILTER_GZIP;
    if (strcmp(media_type, OCI_MEDIA_TYPE_TAR_ZSTD) == 0)
        return ARCHIVE_FILTER_ZSTD;

    return -1;
}

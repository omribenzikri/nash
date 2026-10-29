#include <stdio.h>
#include <string.h>
#include <linux/limits.h>
#include <ocispec/json_common.h>
#include <ocispec/image_spec_schema_image_layout_schema.h>
#include <ocispec/image_spec_schema_image_index_schema.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_content_descriptor.h>
#include <ocispec/image_spec_schema_config_schema.h>
#include "images/oci.h"

#define NSH_OCI_SUPPORTED_IMAGE_LAYOUT_VERSION "1.0.0"

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
        return NULL;
    }

    if (strcmp(layout->image_layout_version, NSH_OCI_SUPPORTED_IMAGE_LAYOUT_VERSION) != 0) {
        fprintf(stderr, "[x] Image contains an unsupported layout version\n");
        return NULL;
    }

    return layout;
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
        return NULL;
    }

    manifest = image_spec_schema_image_manifest_schema_parse_file(manifest_file_path, NULL, &parser_err);
    if (manifest == NULL) {
        fprintf(stderr, "[x] Failed to parse image manifest: %s\n", parser_err);
        return NULL;
    }

    if (strcmp(manifest->config->media_type, OCI_MEDIA_TYPE_CONFIG)!= 0) {
        fprintf(stderr, "[x] Image config reference does not contain a valid media type\n");
        return NULL;
    }

    return manifest;
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

    if (image->index->manifests_len < 1) {
        fprintf(stderr, "[x] Image index does not contain any manifests\n");
        goto error_free_index;
    }

    // TODO: pick a suitable manifest instead of just the first
    manifest_element = image->index->manifests[0];

    if (strcmp(manifest_element->media_type, OCI_MEDIA_TYPE_MANIFEST)!= 0) {
        fprintf(stderr, "[x] Image manifest reference does not contain a valid media type\n");
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

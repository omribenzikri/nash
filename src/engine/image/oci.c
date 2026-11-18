#include <stdlib.h>
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
#include "engine/image/oci/bundle.h"
#include "engine/image/oci.h"

#define OCI_WHITEOUT_PREFIX ".wh."
#define OCI_OPAQUE_WHITEOUT_NAME ".wh..wh..opq"

int nsh_oci_digest_to_path(const char *image_path, const char *digest, char *path, size_t size)
{
    return nsh_oci_bundle_blob_path_resolve(image_path, digest, path, size);
}

int nsh_oci_image_load(oci_image_t *image, const char *path)
{
    image_spec_schema_image_index_schema_manifests_element *manifest_element;

    image->layout = nsh_oci_bundle_layout_load(path);
    if (image->layout == NULL)
        goto error;

    image->index = nsh_oci_bundle_index_load(path);
    if (image->index == NULL)
        goto error_free_layout;

    manifest_element = nsh_oci_find_compat_manifest_element(image->index);
    if (manifest_element == NULL) {
        fprintf(stderr, "[x] Failed to find a compatible manifest in index\n");
        goto error_free_index;
    }

    image->manifest = nsh_oci_bundle_manifest_load(path, manifest_element->digest);
    if (image->manifest == NULL)
        goto error_free_index;

    image->config = nsh_oci_bundle_config_load(path, image->manifest->config->digest);
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

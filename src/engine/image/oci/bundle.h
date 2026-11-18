#ifndef _NASH_ENGINE_IMAGE_OCI_BUNDLE_H
#define _NASH_ENGINE_IMAGE_OCI_BUNDLE_H

#include <stddef.h>
#include <ocispec/image_spec_schema_image_layout_schema.h>
#include <ocispec/image_spec_schema_image_index_schema.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>
#include "engine/image/oci/common.h"

image_spec_schema_image_index_schema_manifests_element
    *nsh_oci_find_compat_manifest_element(
        image_spec_schema_image_index_schema *index);

image_spec_schema_image_layout_schema *nsh_oci_bundle_layout_load(const char *bundle_path);
image_spec_schema_image_index_schema *nsh_oci_bundle_index_load(const char *bundle_path);
image_spec_schema_image_manifest_schema *nsh_oci_bundle_manifest_load(
    const char *bundle_path, const char *digest);
image_spec_schema_config_schema *nsh_oci_bundle_config_load(
    const char *bundle_path, const char *digest);

int nsh_oci_bundle_blob_path_resolve(
    const char *bundle_path, const char *digest,
    char *buf, size_t size);

int nsh_oci_bundle_load_image_meta(nsh_oci_image_meta_t *meta,
    const char *bundle_path, char **out);

#endif

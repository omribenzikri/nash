#ifndef _NASH_ENGINE_IMAGE_OCI_BUNDLE_H
#define _NASH_ENGINE_IMAGE_OCI_BUNDLE_H

#include <stddef.h>
#include <ocispec/image_spec_schema_image_layout_schema.h>
#include <ocispec/image_spec_schema_image_index_schema.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>
#include "engine/image/oci/common.h"

int nsh_oci_bundle_blob_path_resolve(
    const char *bundle_path, const char *digest,
    char *buf, size_t size);

int nsh_oci_bundle_load_image_meta(nsh_oci_image_meta_t *meta,
    const char *bundle_path, char *outb, size_t outl);

int nsh_oci_bundle_create(const char *bundle_path);
int nsh_oci_bundle_add_blob(const char *bundle_path,
    const char *blob_path, const char *digest);
int nsh_oci_bundle_seal(const char *bundle_path, const char *manifest_digest);

#endif

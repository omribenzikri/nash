#ifndef _NASH_ENGINE_IMAGE_OCI_PARSE_H
#define _NASH_ENGINE_IMAGE_OCI_PARSE_H

#include <stddef.h>
#include <ocispec/image_spec_schema_image_layout_schema.h>
#include <ocispec/image_spec_schema_image_index_schema.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>

int nsh_oci_digest_parse(const char *digest,
    char *alg_buf, size_t alg_size,
    char *encoded_buf, size_t encoded_size);

image_spec_schema_image_layout_schema *nsh_oci_layout_parse(const char *path);
image_spec_schema_image_index_schema *nsh_oci_index_parse(const char *path);
image_spec_schema_image_manifest_schema *nsh_oci_manifest_parse(const char *path);
image_spec_schema_config_schema *nsh_oci_config_parse(const char *path);

int nsh_oci_layer_archive_format(const char *media_type);
int nsh_oci_layer_archive_filter(const char *media_type);

#endif

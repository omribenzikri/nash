#ifndef _NASH_ENGINE_IMAGE_OCI_COMMON_H
#define _NASH_ENGINE_IMAGE_OCI_COMMON_H

#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>

typedef struct {
    image_spec_schema_image_manifest_schema *manifest;
    image_spec_schema_config_schema *config;
} nsh_oci_image_meta_t;

void nsh_oci_image_meta_destroy(nsh_oci_image_meta_t *meta);

#endif

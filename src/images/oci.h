#ifndef _NASH_IMAGES_OCI_H
#define _NASH_IMAGES_OCI_H

#include <ocispec/image_spec_schema_image_layout_schema.h>
#include <ocispec/image_spec_schema_image_index_schema.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>

typedef struct {
    image_spec_schema_image_layout_schema *layout;
    image_spec_schema_image_index_schema *index;
    image_spec_schema_image_manifest_schema *manifest;
} nsh_oci_image_t;

int nsh_load_oci_image(nsh_oci_image_t *image, const char *path);
void nsh_free_oci_image(nsh_oci_image_t *image);

#endif

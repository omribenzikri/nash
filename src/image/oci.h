#ifndef _NASH_IMAGE_OCI_H
#define _NASH_IMAGE_OCI_H

#include <stddef.h>
#include <ocispec/image_spec_schema_image_layout_schema.h>
#include <ocispec/image_spec_schema_image_index_schema.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>

#define OCI_MEDIA_TYPE_MANIFEST "application/vnd.oci.image.manifest.v1+json"
#define OCI_MEDIA_TYPE_CONFIG "application/vnd.oci.image.config.v1+json"
#define OCI_MEDIA_TYPE_TAR "application/vnd.oci.image.layer.v1.tar"
#define OCI_MEDIA_TYPE_TAR_GZIP "application/vnd.oci.image.layer.v1.tar+gzip"
#define OCI_MEDIA_TYPE_TAR_ZSTD "application/vnd.oci.image.layer.v1.tar+zstd"

typedef struct {
    image_spec_schema_image_layout_schema *layout;
    image_spec_schema_image_index_schema *index;
    image_spec_schema_image_manifest_schema *manifest;
    image_spec_schema_config_schema *config;
} oci_image_t;

int nsh_oci_image_load(oci_image_t *image, const char *path);
void nsh_oci_image_free(oci_image_t *image);
int nsh_oci_digest_to_path(const char *image_path, const char *digest, char *path, size_t size);

#endif

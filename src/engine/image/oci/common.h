#ifndef _NASH_ENGINE_IMAGE_OCI_COMMON_H
#define _NASH_ENGINE_IMAGE_OCI_COMMON_H

#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>

#define OCI_SCHEMA_VERSION 2

#define OCI_MEDIA_TYPE_MANIFEST "application/vnd.oci.image.manifest.v1+json"
#define OCI_MEDIA_TYPE_CONFIG "application/vnd.oci.image.config.v1+json"
#define OCI_MEDIA_TYPE_TAR "application/vnd.oci.image.layer.v1.tar"
#define OCI_MEDIA_TYPE_TAR_GZIP "application/vnd.oci.image.layer.v1.tar+gzip"
#define OCI_MEDIA_TYPE_TAR_ZSTD "application/vnd.oci.image.layer.v1.tar+zstd"

#define OCI_HASH_ALG_SHA256 "sha256"
#define OCI_HASH_ALG_SHA512 "sha512"
#define OCI_HASH_ALG_BLAKE3 "blake3"

#define OCI_WHITEOUT_PREFIX ".wh."
#define OCI_OPAQUE_WHITEOUT_NAME ".wh..wh..opq"

typedef struct {
    image_spec_schema_image_manifest_schema *manifest;
    image_spec_schema_config_schema *config;
} nsh_oci_image_meta_t;

void nsh_oci_image_meta_destroy(nsh_oci_image_meta_t *meta);

#endif

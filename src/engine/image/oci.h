#ifndef _NASH_ENGINE_IMAGE_OCI_H
#define _NASH_ENGINE_IMAGE_OCI_H

#include <stddef.h>
#include <stdbool.h>
#include <ocispec/image_spec_schema_image_layout_schema.h>
#include <ocispec/image_spec_schema_image_index_schema.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>
#include <ocispec/image_spec_schema_content_descriptor.h>

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

typedef struct {
    char *ref;
    image_spec_schema_image_manifest_schema *manifest;
    image_spec_schema_config_schema *config;
} nsh_oci_image_meta_t;

int nsh_oci_image_load(oci_image_t *image, const char *path);
void nsh_oci_image_free(oci_image_t *image);
int nsh_oci_digest_to_path(const char *image_path, const char *digest, char *path, size_t size);

int nsh_oci_image_meta_load(nsh_oci_image_meta_t *meta, const char *path);
void nsh_oci_image_meta_destroy(nsh_oci_image_meta_t *meta);

bool nsh_oci_is_whiteout(const char *pathname);
bool nsh_oci_is_valid_whiteout(const char *pathname);
bool nsh_oci_is_regular_whiteout(const char *pathname);
bool nsh_oci_is_opaque_whiteout(const char *pathname);
int nsh_oci_whiteout_to_file(const char *pathname, char *buf, size_t size);

int nsh_oci_import_manifest(const char *repo_path,
    const char *image_path, nsh_oci_image_meta_t *meta);
int nsh_oci_import_config(const char *repo_path,
    const char *image_path, nsh_oci_image_meta_t *meta);
int nsh_oci_import_layers(const char *repo_path,
    const char *image_path, nsh_oci_image_meta_t *meta);

#endif

#ifndef _NASH_IMAGE_ROOTFS_H
#define _NASH_IMAGE_ROOTFS_H

#include <ocispec/image_spec_schema_image_manifest_schema.h>

int nsh_image_layers_extract(
    image_spec_schema_image_manifest_schema *manifest,
    const char *image_path,
    int workdir_fd);

#endif

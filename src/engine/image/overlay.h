#ifndef _NASH_ENGINE_IMAGE_OVERLAY_H
#define _NASH_ENGINE_IMAGE_OVERLAY_H

#include <stddef.h>
#include <sys/sysmacros.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>

#define OVERLAYFS_NAME "overlay"
#define OVERLAYFS_WHITEOUT_DEV makedev(0, 0)
#define OVERLAYFS_WHITEOUT_XATTR "trusted.overlay.whiteout"
#define OVERLAYFS_OPAQUE_XATTR "trusted.overlay.opaque"

int nsh_overlay_whiteout_create(const char *path);
int nsh_overlay_opaque_set(const char *path);

int nsh_overlay_create(
    image_spec_schema_image_manifest_schema *manifest,
    const char *workspace_path,
    char *rootfs_path, size_t size
);

#endif

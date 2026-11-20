#ifndef _NASH_ENGINE_IMAGE_OVERLAY_H
#define _NASH_ENGINE_IMAGE_OVERLAY_H

#include <stddef.h>
#include <sys/sysmacros.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <archive_entry.h>

#define OVERLAYFS_NAME "overlay"
#define OVERLAYFS_WHITEOUT_DEV makedev(0, 0)
#define OVERLAYFS_WHITEOUT_XATTR "trusted.overlay.whiteout"
#define OVERLAYFS_OPAQUE_XATTR "trusted.overlay.opaque"

bool nsh_overlay_archive_entry_is_whiteout(struct archive_entry *entry);
bool nsh_overlay_archive_entry_is_opaque(struct archive_entry *entry);

int nsh_overlay_whiteout_create(const char *path);
int nsh_overlay_opaque_set(const char *path);

int nsh_overlay_create(
    const char *target, const char *workspace,
    char **snapshots_paths, size_t snapshots_paths_len);

#endif

#ifndef _NASH_IMAGE_OVERLAY_H
#define _NASH_IMAGE_OVERLAY_H

#include <sys/sysmacros.h>

#define OVERLAYFS_WHITEOUT_DEV makedev(0, 0)
#define OVERLAYFS_WHITEOUT_XATTR "trusted.overlay.whiteout"
#define OVERLAYFS_OPAQUE_XATTR "trusted.overlay.opaque"

int nsh_overlay_whiteout_create(const char *path);
int nsh_overlay_opaque_set(const char *path);

#endif

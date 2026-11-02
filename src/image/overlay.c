#include <stdio.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include "utils/filesystem.h"
#include "image/overlay.h"

int nsh_overlay_whiteout_create(const char *path)
{
    int err;

    err = create_with_xattr(path, S_IRUSR,
        OVERLAYFS_WHITEOUT_XATTR, "y", 1, 0);
    if (err) {
        perror("[x] Failed to create whiteout");
        return -1;
    }

    return 0;
}

int nsh_overlay_opaque_set(const char *path)
{
    int err;

    err = setxattr(path, OVERLAYFS_OPAQUE_XATTR, "y", 1, 0);
    if (err) {
        perror("[x] Failed to set opaque xattr");
        return -1;
    }

    return 0;
}

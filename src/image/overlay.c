#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/mount.h>
#include <linux/mount.h>
#include <linux/limits.h>
#include "common/constants.h"
#include "utils/filesystem.h"
#include "image/overlay.h"

int nsh_overlay_whiteout_create(const char *path)
{
    int err;
    char dir_name[PATH_MAX];

    split_path(path, dir_name, sizeof(dir_name), NULL, 0);

    err = create_with_xattr(path, S_IRUSR,
        OVERLAYFS_WHITEOUT_XATTR, "y", 1, 0);
    if (err) {
        perror("[x] Failed to create whiteout");
        return -1;
    }

    err = setxattr(dir_name, OVERLAYFS_OPAQUE_XATTR, "x", 1, 0);
    if (err) {
        perror("[x] Failed to set opaque xattr");
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

static inline void nsh_resolve_lowerdir_path(
    const char *workspace_path, const char *layer_digest,
    char *buf, size_t size)
{
    snprintf(buf, size, "%s/%s/%s",
        workspace_path, NSH_FS_WORKDIR_OVERLAY_SNAPSHOTS,
        layer_digest);
}

static inline void nsh_resolve_upperdir_path(
    const char *workspace_path, char *buf, size_t size)
{
    snprintf(buf, size, "%s/%s",
        workspace_path, NSH_FS_WORKDIR_OVERLAY_UPPER);
}

static inline void nsh_resolve_workdir_path(
    const char *workspace_path, char *buf, size_t size)
{
    snprintf(buf, size, "%s/%s",
        workspace_path, NSH_FS_WORKDIR_OVERLAY_WORKDIR);
}

static inline void nsh_resolve_rootfs_path(
    const char *workspace_path, char *buf, size_t size)
{
    snprintf(buf, size, "%s/%s",
        workspace_path, NSH_FS_WORKDIR_ROOTFS);
}

static int nsh_lowerdir_add(int fsfd,
    const char *workspace_path, const char *layer_digest)
{
    char path[PATH_MAX];
    int err;

    nsh_resolve_lowerdir_path(
        workspace_path, layer_digest,
        path, sizeof(path));

    err = fsconfig(fsfd, FSCONFIG_SET_STRING, "lowerdir+", path, 0);
    if (err) {
        perror("[x] Failed to configure lowerdir");
        return -1;
    }

    return 0;
}

static int nsh_upperdir_set(int fsfd, const char *workspace_path)
{
    char path[PATH_MAX];
    int err;

    nsh_resolve_upperdir_path(workspace_path, path, sizeof(path));

    err = fsconfig(fsfd, FSCONFIG_SET_STRING, "upperdir", path, 0);
    if (err) {
        perror("[x] Failed to configure upperdir");
        return -1;
    }

    return 0;
}

static int nsh_workdir_set(int fsfd, const char *workspace_path)
{
    char path[PATH_MAX];
    int err;

    nsh_resolve_workdir_path(workspace_path, path, sizeof(path));

    err = fsconfig(fsfd, FSCONFIG_SET_STRING, "workdir", path, 0);
    if (err) {
        perror("[x] Failed to configure workdir");
        return -1;
    }

    return 0;
}

static int nsh_overlay_features_enable(int fsfd)
{
    int err;

    err = fsconfig(fsfd, FSCONFIG_SET_STRING, "redirect_dir", "on", 0);
    if (err) {
        perror("[x] Failed to configure \"redirect_dir\" feature");
        return -1;
    }

    err = fsconfig(fsfd, FSCONFIG_SET_STRING, "index", "on", 0);
    if (err) {
        perror("[x] Failed to configure \"index\" feature");
        return -1;
    }

    return 0;
}

static int nsh_overlay_config(int fsfd,
    image_spec_schema_image_manifest_schema *manifest,
    const char *workspace_path)
{
    int err;

    for (int i = manifest->layers_len - 1; i >= 0; i--) {
        err = nsh_lowerdir_add(fsfd, workspace_path, manifest->layers[i]->digest);
        if (err)
            return -1;
    }

    err = nsh_upperdir_set(fsfd, workspace_path);
    if (err)
        return -1;

    err = nsh_workdir_set(fsfd, workspace_path);
    if (err)
        return -1;

    err = nsh_overlay_features_enable(fsfd);
    if (err)
        return -1;

    err = fsconfig(fsfd, FSCONFIG_CMD_CREATE, NULL, NULL, 0);
    if (err) {
        perror("[x] Failed to create overlayfs instance");
        return -1;
    }

    return 0;
}

static int nsh_overlay_mount(int fsfd, const char *target)
{
    int mntfd;
    int ret = 0;
    int err;

    mntfd = fsmount(fsfd, 0, 0);
    if (mntfd == -1) {
        perror("[x] Failed to instantiate mount object");
        ret = -1;
        goto out;
    }

    err = move_mount(mntfd, "", AT_FDCWD, target, MOVE_MOUNT_F_EMPTY_PATH);
    if (err) {
        perror("[x] Failed to attach mount object");
        ret = -1;
        goto out_close_mntfd;
    }

out_close_mntfd:
    err = close(mntfd);
    if (err) {
        perror("[x] Failed to close mount object");
        ret = -1;
    }
out:
    return ret;
}

int nsh_overlay_create(
    image_spec_schema_image_manifest_schema *manifest,
    const char *workspace_path,
    char *rootfs_path, size_t size)
{
    int fsfd;
    int ret = 0;
    int err;

    fsfd = fsopen(OVERLAYFS_NAME, 0);
    if (fsfd == -1) {
        perror("[x] Failed to open filesystem context");
        ret = -1;
        goto out;
    }

    err = nsh_overlay_config(fsfd, manifest, workspace_path);
    if (err) {
        ret = -1;
        goto out_close_fsfd;
    }

    nsh_resolve_rootfs_path(workspace_path, rootfs_path, size);

    err = nsh_overlay_mount(fsfd, rootfs_path);
    if (err) {
        ret = -1;
        goto out_close_fsfd;
    }

out_close_fsfd:
    err = close(fsfd);
    if (err) {
        perror("[x] Failed to close filesystem context");
        ret = -1;
    }
out:
    return ret;
}

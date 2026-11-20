#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/mount.h>
#include <linux/mount.h>
#include <linux/limits.h>
#include <archive_entry.h>
#include "common/utils/archive.h"
#include "common/utils/filesystem.h"
#include "engine/image/overlay.h"

bool nsh_overlay_archive_entry_is_whiteout(struct archive_entry *entry)
{
    if (archive_entry_filetype(entry) == AE_IFCHR &&
        archive_entry_rdev(entry) == OVERLAYFS_WHITEOUT_DEV)
            return true;

    if (archive_entry_xattr_exists(entry, OVERLAYFS_WHITEOUT_XATTR))
        return true;

    return false;
}

bool nsh_overlay_archive_entry_is_opaque(struct archive_entry *entry)
{
    return archive_entry_xattr_exists(entry, OVERLAYFS_OPAQUE_XATTR);
}

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

static int nsh_overlay_workspace_prepare(
    const char *upper_path, const char *workdir_path)
{
    int err;

    err = mkdir(upper_path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create overlayfs upperdir");
        return -1;
    }

    err = mkdir(workdir_path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create overlayfs workdir");
        return -1;
    }

    return 0;
}

static int nsh_overlay_config_features(int fsfd)
{
    int err;

    err = fsconfig(fsfd, FSCONFIG_SET_STRING, "redirect_dir", "on", 0);
    if (err) {
        perror("[x] Failed to configure overlayfs \"redirect_dir\" feature");
        return -1;
    }

    err = fsconfig(fsfd, FSCONFIG_SET_STRING, "index", "on", 0);
    if (err) {
        perror("[x] Failed to configure overlayfs \"index\" feature");
        return -1;
    }

    return 0;
}

static int nsh_overlay_config(int fsfd,
    const char *upper_path, const char *workdir_path,
    char **snapshots_paths, size_t snapshots_paths_len)
{
    int err;

    for (int i = snapshots_paths_len - 1; i >= 0; i--) {
        err = fsconfig(fsfd, FSCONFIG_SET_STRING, "lowerdir+", snapshots_paths[i], 0);
        if (err) {
            perror("[x] Failed to configure overlayfs lowerdir");
            return -1;
        }
    }

    err = fsconfig(fsfd, FSCONFIG_SET_STRING, "upperdir", upper_path, 0);
    if (err) {
        perror("[x] Failed to configure overlayfs upperdir");
        return -1;
    }

    err = fsconfig(fsfd, FSCONFIG_SET_STRING, "workdir", workdir_path, 0);
    if (err) {
        perror("[x] Failed to configure overlayfs workdir");
        return -1;
    }

    err = nsh_overlay_config_features(fsfd);
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
    const char *target, const char *workspace,
    char **snapshots_paths, size_t snapshots_paths_len)
{
    char upper_path[PATH_MAX];
    char workdir_path[PATH_MAX];
    int fsfd;
    int err;
    int ret = 0;

    snprintf(upper_path, sizeof(upper_path), "%s/upper", workspace);
    snprintf(workdir_path, sizeof(workdir_path), "%s/workdir", workspace);

    err = nsh_overlay_workspace_prepare(upper_path, workdir_path);
    if (err) {
        ret = -1;
        goto out;
    }

    fsfd = fsopen(OVERLAYFS_NAME, 0);
    if (fsfd == -1) {
        perror("[x] Failed to open filesystem context");
        ret = -1;
        goto out;
    }

    err = nsh_overlay_config(fsfd, upper_path, workdir_path, snapshots_paths, snapshots_paths_len);
    if (err) {
        ret = -1;
        goto out_close_fsfd;
    }

    err = nsh_overlay_mount(fsfd, target);
    if (err) {
        ret = -1;
        goto out_close_fsfd;
    }

out_close_fsfd:
    close(fsfd);
out:
    return ret;
}

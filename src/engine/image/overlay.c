#define _GNU_SOURCE

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/mount.h>
#include <linux/mount.h>
#include <linux/limits.h>
#include <archive.h>
#include <archive_entry.h>
#include "utils/file.h"
#include "engine/image/overlay.h"

bool nsh_overlay_archive_entry_is_whiteout(struct archive_entry *entry)
{
    const char *xname;
    const void *xvalue;
    size_t xsize;
    int retcode;

    if (archive_entry_filetype(entry) == AE_IFCHR &&
        archive_entry_rdev(entry) == OVERLAYFS_WHITEOUT_DEV)
            return true;

    archive_entry_xattr_reset(entry);

    while ((retcode = archive_entry_xattr_next(
        entry, &xname, &xvalue, &xsize)) == ARCHIVE_OK) {
            if (strcmp(xname, OVERLAYFS_WHITEOUT_XATTR) == 0)
                return true;
        }

    return false;
}

bool nsh_overlay_archive_entry_is_opaque(struct archive_entry *entry)
{
    const char *xname;
    const void *xvalue;
    size_t xsize;
    int retcode;

    archive_entry_xattr_reset(entry);

    while ((retcode = archive_entry_xattr_next(
        entry, &xname, &xvalue, &xsize)) == ARCHIVE_OK) {
            if (strcmp(xname, OVERLAYFS_OPAQUE_XATTR) == 0)
                return true;
        }

    return false;
}

// Guarantees atomicity (file cannot be created without the xattr)
static int nsh_overlay_whiteout_create_file(const char *dir, const char *name)
{
    int dirfd;
    int wofd;
    int err;
    int ret = 0;

    dirfd = open(dir, O_RDONLY | O_DIRECTORY);
    if (dirfd == -1) {
        perror("[x] Failed to open whiteout parent directory");
        ret = -1;
        goto out;
    }

    wofd = openat(dirfd, ".", O_TMPFILE | O_WRONLY, S_IRUSR);
    if (dirfd == -1) {
        perror("[x] Failed to create whiteout");
        ret = -1;
        goto out_close_dirfd;
    }

    err = fsetxattr(wofd, OVERLAYFS_WHITEOUT_XATTR, "y", 1, 0);
    if (err) {
        perror("[x] Failed to set whiteout xattr");
        ret = -1;
        goto out_close_wofd;
    }

    err = linkat(wofd, "", dirfd, name, AT_EMPTY_PATH);
    if (err) {
        perror("[x] Failed to link whiteout");
        ret = -1;
        goto out_close_wofd;
    }

out_close_wofd:
    close(wofd);
out_close_dirfd:
    close(dirfd);
out:
    return ret;
}

int nsh_overlay_whiteout_create(const char *path)
{
    char dir_name[PATH_MAX];
    char base_name[PATH_MAX];
    int err;

    split_path(path,
        dir_name, sizeof(dir_name),
        base_name, sizeof(base_name));

    err = nsh_overlay_whiteout_create_file(dir_name, base_name);
    if (err)
        return -1;

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

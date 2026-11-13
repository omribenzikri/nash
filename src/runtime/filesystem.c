#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <linux/limits.h>
#include "utils/filesystem.h"
#include "runtime/filesystem.h"
#include "utils/macros.h"

static const mode_t NSH_DEFAULT_CHAR_DEV_MODE =
    S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

static const nsh_mount_t nsh_root_mounts[] = {
    {
        .source = "proc",
        .destination = "/proc",
        .type = "proc",
        .mode = S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
    },
    {
        .source = "sysfs",
        .destination = "/sys",
        .type = "sysfs",
        .mode = S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
    },
    {
        .source = "tmpfs",
        .destination = "/dev",
        .type = "tmpfs",
        .mode = S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX,
    },
};

static const nsh_mount_t nsh_sys_mounts[] = {
    {
        .source = "cgroup2",
        .destination = "/sys/fs/cgroup",
        .type = "cgroup2",
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
    }
};

static const nsh_mount_t nsh_dev_mounts[] = {
    {
        .source = "devpts",
        .destination = "/dev/pts",
        .type = "devpts",
        .mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IWOTH
    },
    {
        .source = "mqueue",
        .destination = "/dev/mqueue",
        .type = "mqueue",
        .mode = S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX,
    },
    {
        .source = "shm",
        .destination = "/dev/shm",
        .type = "tmpfs",
        .mode = S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX,
    },
};

static const nsh_node_t nsh_dev_nodes[] = {
    {
        .path = "/dev/zero",
        .mode = NSH_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 1,
        .dev_minor = 5
    },
    {
        .path = "/dev/full",
        .mode = NSH_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 1,
        .dev_minor = 7
    },
    {
        .path = "/dev/null",
        .mode = NSH_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 1,
        .dev_minor = 3
    },
    {
        .path = "/dev/random",
        .mode = NSH_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 1,
        .dev_minor = 8
    },
    {
        .path = "/dev/urandom",
        .mode = NSH_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 1,
        .dev_minor = 8
    },
    {
        .path = "/dev/tty",
        .mode = NSH_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 5,
        .dev_minor = 0
    }
};

static const nsh_symlink_t nsh_dev_symlinks[] = {
    { .from = "/dev/pts/ptmx",   .to = "/dev/ptmx"   },
    { .from = "/proc/self/fd",   .to = "/dev/fd"     },
    { .from = "/proc/kcore",     .to = "/dev/core"   },
    { .from = "/proc/self/fd/0", .to = "/dev/stdin"  },
    { .from = "/proc/self/fd/1", .to = "/dev/stdout" },
    { .from = "/proc/self/fd/2", .to = "/dev/stderr" }
};

static int nsh_mount(const nsh_mount_t *mnt)
{
    int err;

    err = mkdir_if_missing(mnt->destination, mnt->mode);
    if (err) {
        perror("[x] Failed to create mount destination");
        return -1;
    }

    err = mount(mnt->source, mnt->destination,
                mnt->type, mnt->flags, mnt->data);
    if (err) {
        perror("[x] Failed to mount");
        return -1;
    }

    return 0;
}

static int nsh_mount_multiple(const nsh_mount_t *mounts, size_t len)
{
    int err;

    for (size_t i = 0; i < len; i ++) {
        err = nsh_mount(&mounts[i]);
        if (err)
            return -1;
    }

    return 0;
}

static int nsh_node_create(const nsh_node_t *node)
{
    dev_t dev = makedev(node->dev_major, node->dev_minor);
    int err;

    err = mknod(node->path, node->mode, dev);
    if (err) {
        perror("[x] Failed to create node");
        return -1;
    }

    return 0;
}

static int nsh_node_create_multiple(const nsh_node_t *nodes, size_t len)
{
    int err;

    for (size_t i = 0; i < len; i ++) {
        err = nsh_node_create(&nodes[i]);
        if (err)
            return -1;
    }

    return 0;
}

static int nsh_symlink_create(const nsh_symlink_t *sym)
{
    int err;

    err = symlink(sym->from, sym->to);
    if (err) {
        perror("[x] Failed to symlink");
        return -1;
    }

    return 0;
}

static int nsh_symlink_create_multiple(const nsh_symlink_t *symlinks, size_t len)
{
    int err;

    for (size_t i = 0; i < len; i ++) {
        err = nsh_symlink_create(&symlinks[i]);
        if (err)
            return -1;
    }

    return 0;
}

static int nsh_fs_root_prepare()
{
    size_t root_mounts_len = ARRAY_LEN(nsh_root_mounts);
    int err;

    err = nsh_mount_multiple(nsh_root_mounts, root_mounts_len);
    if (err)
        return -1;

    return 0;
}

static int nsh_fs_sys_prepare()
{
    size_t sys_mounts_len = ARRAY_LEN(nsh_sys_mounts);
    int err;

    err = nsh_mount_multiple(nsh_sys_mounts, sys_mounts_len);
    if (err)
        return -1;

    return 0;
}

static int nsh_fs_dev_prepare()
{
    size_t mounts_len = ARRAY_LEN(nsh_dev_mounts);
    size_t nodes_len = ARRAY_LEN(nsh_dev_nodes);
    size_t symlinks_len = ARRAY_LEN(nsh_dev_symlinks);
    int err;

    err = nsh_mount_multiple(nsh_dev_mounts, mounts_len);
    if (err)
        return -1;

    err = nsh_node_create_multiple(nsh_dev_nodes, nodes_len);
    if (err)
        return -1;

    err = nsh_symlink_create_multiple(nsh_dev_symlinks, symlinks_len);
    if (err)
        return -1;

    return 0;
}

int nsh_fs_prepare()
{
    int err;

    umask(0);

    err = nsh_fs_root_prepare();
    if (err)
        return -1;

    err = nsh_fs_sys_prepare();
    if (err)
        return -1;

    err = nsh_fs_dev_prepare();
    if (err)
        return -1;

    return 0;
}

int nsh_fs_pivot_root(const char *root, const char *cwd)
{
    const char *old_root_rel = "/old-rootfs";
    char old_root_abs[PATH_MAX];
    int ret = 0;
    int err;

    snprintf(old_root_abs, sizeof(old_root_abs), "%s/%s", root, old_root_rel);

    err = mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL);
    if (err) {
        perror("[x] Failed to remount \"/\" as private");
        return -1;
    }

    err = mount(root, root, NULL, MS_BIND, NULL);
    if (err) {
        perror("[x] Failed to create a bind mount for image filesystem");
        return -1;
    }

    err = mkdir(old_root_abs, S_IRWXU);
    if (err) {
        perror("[x] Failed to create directory for old rootfs");
        return -1;
    }

    err = syscall(SYS_pivot_root, root, old_root_abs);
    if (err) {
        perror("[x] Failed to pivot rootfs");

        err = rmdir(old_root_abs);
        if (err)
            perror("[x] Failed to delete old rootfs directory");

        return -1;
    }

    err = chdir(cwd);
    if (err) {
        perror("[x] Failed to change working directory");
        ret = -1;
    }

    err = umount2(old_root_rel, MNT_DETACH);
    if (err) {
        perror("[x] Failed to unmount old rootfs");
        ret = -1;
    }

    err = rmdir(old_root_rel);
    if (err) {
        perror("[x] Failed to delete old rootfs mount system");
        ret = -1;
    }

    return ret;
}

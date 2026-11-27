#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include "utils/macros.h"
#include "utils/file.h"
#include "container/filesystem.h"

static const mode_t CNTR_DEFAULT_CHAR_DEV_MODE =
    S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

static const cntr_mount_t cntr_root_mounts[] = {
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

static const cntr_mount_t cntr_sys_mounts[] = {
    {
        .source = "cgroup2",
        .destination = "/sys/fs/cgroup",
        .type = "cgroup2",
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
    }
};

static const cntr_mount_t cntr_dev_mounts[] = {
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

static const cntr_node_t cntr_dev_nodes[] = {
    {
        .path = "/dev/zero",
        .mode = CNTR_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 1,
        .dev_minor = 5
    },
    {
        .path = "/dev/full",
        .mode = CNTR_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 1,
        .dev_minor = 7
    },
    {
        .path = "/dev/null",
        .mode = CNTR_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 1,
        .dev_minor = 3
    },
    {
        .path = "/dev/random",
        .mode = CNTR_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 1,
        .dev_minor = 8
    },
    {
        .path = "/dev/urandom",
        .mode = CNTR_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 1,
        .dev_minor = 8
    },
    {
        .path = "/dev/tty",
        .mode = CNTR_DEFAULT_CHAR_DEV_MODE,
        .dev_major = 5,
        .dev_minor = 0
    }
};

static const cntr_symlink_t cntr_dev_symlinks[] = {
    { .from = "/dev/pts/ptmx",   .to = "/dev/ptmx"   },
    { .from = "/proc/self/fd",   .to = "/dev/fd"     },
    { .from = "/proc/kcore",     .to = "/dev/core"   },
    { .from = "/proc/self/fd/0", .to = "/dev/stdin"  },
    { .from = "/proc/self/fd/1", .to = "/dev/stdout" },
    { .from = "/proc/self/fd/2", .to = "/dev/stderr" }
};

static int cntr_mount(const cntr_mount_t *mounts, size_t len)
{
    int err;

    for (size_t i = 0; i < len; i ++) {
        err = mkdir_missing(mounts[i].destination, mounts[i].mode);
        if (err) {
            perror("[x] Failed to create mount directory");
            return -1;
        }

        err = mount(mounts[i].source, mounts[i].destination,
            mounts[i].type, mounts[i].flags, mounts[i].data);
        if (err) {
            perror("[x] Failed to mount");
            return -1;
        }
    }

    return 0;
}

static int cntr_mknod(const cntr_node_t *nodes, size_t len)
{
    dev_t dev;
    int err;

    for (size_t i = 0; i < len; i ++) {
        dev = makedev(nodes[i].dev_major, nodes[i].dev_minor);

        err = mknod(nodes[i].path, nodes[i].mode, dev);
        if (err) {
            perror("[x] Failed to create node");
            return -1;
        }
    }

    return 0;
}

static int cntr_symlink(const cntr_symlink_t *symlinks, size_t len)
{
    int err;

    for (size_t i = 0; i < len; i ++) {
        err = symlink(symlinks[i].from, symlinks[i].to);
        if (err) {
            perror("[x] Failed to symlink");
            return -1;
        }
    }

    return 0;
}

static int cntr_rootfs_init()
{
    int err;

    err = cntr_mount(cntr_root_mounts, ARRAY_LEN(cntr_root_mounts));
    if (err)
        return -1;

    return 0;
}

static int cntr_sysfs_init()
{
    int err;

    err = cntr_mount(cntr_sys_mounts, ARRAY_LEN(cntr_sys_mounts));
    if (err)
        return -1;

    return 0;
}

static int cntr_devfs_init()
{
    int err;

    err = cntr_mount(cntr_dev_mounts, ARRAY_LEN(cntr_dev_mounts));
    if (err)
        return -1;

    err = cntr_mknod(cntr_dev_nodes, ARRAY_LEN(cntr_dev_nodes));
    if (err)
        return -1;

    err = cntr_symlink(cntr_dev_symlinks, ARRAY_LEN(cntr_dev_symlinks));
    if (err)
        return -1;

    return 0;
}

int cntr_filesystem_init()
{
    int err;

    umask(0);

    err = cntr_rootfs_init() ||
          cntr_sysfs_init() ||
          cntr_devfs_init();

    return err ? -1 : 0;
}

// Ensures that name conflicts with pre-existing image files
// or directories are practically impossible
static void cntr_old_root_name_generate(char *buf, size_t size)
{
    uuid_t uuid;
    char name[UUID_STR_LEN];

    uuid_generate_random(uuid);
    uuid_unparse_lower(uuid, name);
    snprintf(buf, size, "%s", name);
}

int cntr_pivot_root(const char *root, const char *cwd)
{
    char old_root_name[NAME_MAX];
    char old_root_path[PATH_MAX];
    int err;
    int ret = 0;

    cntr_old_root_name_generate(old_root_name, sizeof(old_root_name));
    snprintf(old_root_path, sizeof(old_root_path), "%s/%s", root, old_root_name);

    err = mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL);
    if (err) {
        perror("[x] Failed to remount \"/\" as private");
        return -1;
    }

    err = mount(root, root, NULL, MS_BIND, NULL);
    if (err) {
        perror("[x] Failed to create a bind mount at rootfs");
        return -1;
    }

    err = mkdir(old_root_path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create old rootfs directory");
        return -1;
    }

    err = syscall(SYS_pivot_root, root, old_root_path);
    if (err) {
        perror("[x] Failed to pivot root");

        err = rmdir(old_root_path);
        if (err)
            perror("[x] Failed to delete old rootfs directory");

        return -1;
    }

    err = chdir(cwd);
    if (err) {
        perror("[x] Failed to change working directory");
        ret = -1;
    }

    err = umount2(old_root_path, MNT_DETACH);
    if (err) {
        perror("[x] Failed to unmount old rootfs");
        ret = -1;
    }

    err = rmdir(old_root_path);
    if (err) {
        perror("[x] Failed to delete old rootfs directory");
        ret = -1;
    }

    return ret;
}

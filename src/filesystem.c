#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <linux/limits.h>
#include "filesystem.h"

int nsh_create_dev_mounts()
{
    int err;

    err = mkdir("/dev/pts", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IWOTH);
    if (err) {
        perror("[x] Failed to create /dev/pts");
        return -1;
    }

    err = mount("devpts", "/dev/pts", "devpts", 0, NULL);
    if (err) {
        perror("[x] Failed to mount devpts at /dev/pts");
        return -1;
    }

    err = mkdir("/dev/mqueue", S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX);
    if (err) {
        perror("[x] Failed to create /dev/mqueue");
        return -1;
    }

    err = mount("mqueue", "/dev/mqueue", "mqueue", 0, NULL);
    if (err) {
        perror("[x] Failed to mount mqueue at /dev/mqueue");
        return -1;
    }

    err = mkdir("/dev/shm", S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX);
    if (err) {
        perror("[x] Failed to create /dev/shm");
        return -1;
    }

    err = mount("shm", "/dev/shm", "tmpfs", 0, NULL);
    if (err) {
        perror("[x] Failed to mount tmpfs at /dev/shm");
        return -1;
    }

    return 0;
}

int nsh_create_dev_nodes()
{
    const mode_t mode = S_IFCHR | S_IRUSR | S_IWUSR |
                        S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    int err;

    err = mknod("/dev/zero", mode, makedev(1, 5));
    if (err) {
        perror("[x] Failed to create /dev/zero");
        return -1;
    }

    err = mknod("/dev/full", mode, makedev(1, 7));
    if (err) {
        perror("[x] Failed to create /dev/full");
        return -1;
    }

    err = mknod("/dev/null", mode, makedev(1, 3));
    if (err) {
        perror("[x] Failed to create /dev/null");
        return -1;
    }

    err = mknod("/dev/random", mode, makedev(1, 8));
    if (err) {
        perror("[x] Failed to create /dev/random");
        return -1;
    }

    err = mknod("/dev/urandom", mode, makedev(1, 8));
    if (err) {
        perror("[x] Failed to create /dev/urandom");
        return -1;
    }

    err = mknod("/dev/tty", mode, makedev(5, 0));
    if (err) {
        perror("[x] Failed to create /dev/tty");
        return -1;
    }

    return 0;
}

int nsh_create_dev_symlinks()
{
    int err;

    err = symlink("/dev/pts/ptmx", "/dev/ptmx");
    if (err) {
        perror("[x] Failed to symlink /dev/ptmx");
        return -1;
    }

    err = symlink("/proc/self/fd", "/dev/fd");
    if (err) {
        perror("[x] Failed to symlink /dev/fd");
        return -1;
    }

    err = symlink("/proc/kcore", "/dev/core");
    if (err) {
        perror("[x] Failed to symlink /dev/core");
        return -1;
    }

    err = symlink("/proc/self/fd/0", "/dev/stdin");
    if (err) {
        perror("[x] Failed to symlink /dev/stdin");
        return -1;
    }

    err = symlink("/proc/self/fd/1", "/dev/stdout");
    if (err) {
        perror("[x] Failed to symlink /dev/stdout");
        return -1;
    }

    err = symlink("/proc/self/fd/2", "/dev/stderr");
    if (err) {
        perror("[x] Failed to symlink /dev/stderr");
        return -1;
    }

    return 0;
}

int nsh_populate_dev()
{
    int err;

    err = nsh_create_dev_mounts();
    if (err)
        return -1;

    err = nsh_create_dev_nodes();
    if (err)
        return -1;

    err = nsh_create_dev_symlinks();
    if (err)
        return -1;

    return 0;
}

int nsh_mount_essentials()
{
    int err;

    err = mount("proc", "/proc", "proc", 0, NULL);
    if (err) {
        perror("[x] Failed to mount procfs at /proc");
        return -1;
    }

    err = mount("sysfs", "/sys", "sysfs", 0, NULL);
    if (err) {
        perror("[x] Failed to mount sysfs at /sys");
        return -1;
    }

    err = mount("tmpfs", "/dev", "tmpfs", 0, NULL);
    if (err) {
        perror("[x] Failed to mount tmpfs at /dev");
        return -1;
    }

    return 0;
}

int nsh_pivot_root(const char *root)
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

    err = chdir("/");
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

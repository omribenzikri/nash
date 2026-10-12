#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <linux/limits.h>

const int NSH_STACK_SIZE = 1024 * 1024;

typedef struct  {
    const char *hostname;
    const char *image_path;
    const char *exec_path;
    char **exec_argv;
    char **exec_envp;
} nsh_container_config_t;


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

int nsh_container_startup(void *data)
{
    nsh_container_config_t *config = data;
    int err;

    umask(0);

    err = sethostname(config->hostname, strlen(config->hostname));
    if (err) {
        perror("[x] Failed to set container hostname");
        return  -1;
    }

    err = nsh_pivot_root(config->image_path);
    if (err)
        return -1;

    err = nsh_mount_essentials();
    if (err)
        return -1;

    err = nsh_populate_dev();
    if (err)
        return -1;

    execve(config->exec_path, config->exec_argv, config->exec_argv);

    perror("[x] Failed to execute container program");
    return -1;
}

int nsh_create_container(nsh_container_config_t *config)
{
    uint8_t *stack;
    uint8_t *stack_top;
    int flags;
    int child_tid;
    int ret = 0;

    printf("[+] Setting up a stack of size: %d bytes\n", NSH_STACK_SIZE);

    stack = malloc(NSH_STACK_SIZE);
    if (stack == NULL) {
        perror("[x] Stack allocation failed");
        return -1;
    }
    stack_top = stack + NSH_STACK_SIZE;

    flags = SIGCHLD | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWIPC;
    child_tid = clone(nsh_container_startup, stack_top, flags, config);
    if (child_tid == -1) {
        perror("[x] Clone failed");
        ret = -1;
        goto out;
    }

    printf("[+] Successfully cloned child\n");

    if (waitpid(child_tid, NULL, 0) == -1) {
        perror("[x] Awaiting child failed");
        ret = -1;
        goto out;
    }

out:
    free(stack);
    return ret;
}

int main(int argc, char **argv)
{
    nsh_container_config_t config;
    char *envp[] = {NULL};
    int err;

    if (argc < 3) {
        fprintf(stderr, "Usage: nash IMAGE COMMAND [ARG...]\n");
        return EXIT_FAILURE;
    }

    config.hostname = "container";
    config.image_path = argv[1];
    config.exec_path = argv[2];
    config.exec_argv = argv + 2;
    config.exec_envp = envp;

    err = nsh_create_container(&config);
    if (err)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

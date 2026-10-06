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
#include <linux/limits.h>

const int NSH_STACK_SIZE = 1024 * 1024;

typedef struct  {
    const char *hostname;
    const char *image_path;
    const char *exec_path;
    char **exec_argv;
    char **exec_envp;
} nsh_container_config_t;


int nsh_pivot_root(const char *root)
{
    const char *old_root_rel = "/old-rootfs";
    char old_root_abs[PATH_MAX];
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
        return -1;
    }

    err = chdir("/");
    if (err) {
        perror("[x] Failed to change working directory");
        return -1;
    }

    err = umount2(old_root_rel, MNT_DETACH);
    if (err) {
        perror("[x] Failed to unmount old rootfs");
        return -1;
    }

    err = rmdir(old_root_rel);
    if (err) {
        perror("[x] Failed to delete old rootfs mount system");
        return -1;
    }

    return 0;
}

int nsh_container_startup(void *data)
{
    nsh_container_config_t *config = data;
    int err;

    err = sethostname(config->hostname, strlen(config->hostname));
    if (err) {
        perror("[x] Failed to set container hostname");
        return  -1;
    }

    err = nsh_pivot_root(config->image_path);
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
        return -1;
    }

    printf("[+] Successfully cloned child\n");

    if (waitpid(child_tid, NULL, 0) == -1) {
        perror("[x] Awaiting child failed");
        return -1;
    }

    return 0;
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

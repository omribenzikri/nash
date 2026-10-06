#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

const int NSH_STACK_SIZE = 1024 * 1024;

typedef struct  {
    const char *hostname;
    const char *exec_path;
    char **exec_argv;
    char **exec_envp;
} nsh_container_config_t;


int nsh_container_startup(void *data)
{
    nsh_container_config_t *config = data;
    int err;

    err = sethostname(config->hostname, strlen(config->hostname));
    if (err) {
        perror("[x] Failed to set container hostname");
        return  -1;
    }

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

    flags = SIGCHLD | CLONE_NEWUTS;
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

    if (argc < 2) {
        fprintf(stderr, "Usage: nash COMMAND [ARG...]\n");
        return EXIT_FAILURE;
    }

    config.hostname = "container";
    config.exec_path = argv[1];
    config.exec_argv = argv + 1;
    config.exec_envp = envp;

    err = nsh_create_container(&config);
    if (err)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

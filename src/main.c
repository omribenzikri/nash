#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/utsname.h>

const int NSH_STACK_SIZE = 1024 * 1024;

typedef struct  {
    const char *hostname;
    const char *exec_path;
} nsh_container_config_t;


int nsh_container_startup(void *data)
{
    int err;
    nsh_container_config_t *config = data;
    struct utsname uts;

    err = sethostname(config->hostname, strlen(config->hostname));
    if (err) {
        perror("[x] Failed to set container hostname");
        return  -1;
    }

    err = uname(&uts);
    if (err) {
        perror("[x] Failed to fetch system info");
        return -1;
    }
    printf("[+] Hostname in container: %s\n", uts.nodename);

    return 0;
}

int nsh_create_container(nsh_container_config_t *config)
{
    uint8_t *stack;
    uint8_t *stack_top;
    int ns_flags;
    int flags;
    int child_tid;
    int err;
    struct utsname uts;

    printf("[+] Setting up a stack of size: %d bytes\n", NSH_STACK_SIZE);

    stack = malloc(NSH_STACK_SIZE);
    if (stack == NULL) {
        perror("[x] Stack allocation failed");
        return EXIT_FAILURE;
    }
    stack_top = stack + NSH_STACK_SIZE;

    ns_flags = CLONE_NEWUTS;
    flags = ns_flags | SIGCHLD;

    child_tid = clone(nsh_container_startup, stack_top, flags, config);
    if (child_tid == -1) {
        perror("[x] Clone failed");
        return EXIT_FAILURE;
    }

    printf("[+] Successfully cloned child\n");

    err = uname(&uts);
    if (err) {
        perror("[x] Failed to fetch system info");
        return -1;
    }
    printf("[+] Hostname in container: %s\n", uts.nodename);

    return 0;
}

int main()
{
    int err;

    nsh_container_config_t config = {
        .hostname = "container",
        .exec_path = "/bin/bash"
    };

    err = nsh_create_container(&config);
    if (err)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

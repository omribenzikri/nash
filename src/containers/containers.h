#ifndef _NASH_CONTAINERS_CONTAINERS_H
#define _NASH_CONTAINERS_CONTAINERS_H

#include <stdint.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include "containers/resources.h"

#define NSH_HOSTNAME_LEN 13

typedef struct {
    uuid_t id;
    pid_t pid;
} nsh_container_t;

typedef struct {
    char hostname[NSH_HOSTNAME_LEN];
    const char *image_path;
    const char *exec_path;
    char **exec_argv;
    char **exec_envp;
    int cgroup_fd;
    nsh_resources_t resources;
} nsh_container_config_t;

int nsh_create_container(nsh_container_config_t *config, nsh_container_t *container);
void nsh_cleanup_container_resources(nsh_container_t *container);

#endif

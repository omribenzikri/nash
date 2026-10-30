#ifndef _NASH_CONTAINER_H
#define _NASH_CONTAINER_H

#include <limits.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include "runtime/resources.h"

typedef struct {
    uuid_t id;
    pid_t pid;
} nsh_container_t;

typedef struct {
    char *hostname;
    char *image_path;
    char *exec_path;
    char **exec_argv;
    char **exec_envp;
    nsh_resources_t resources;
} nsh_container_config_t;

int nsh_container_create(nsh_container_config_t *config, nsh_container_t *out);
void nsh_container_delete(nsh_container_t *container);

#endif

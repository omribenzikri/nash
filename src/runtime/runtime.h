#ifndef _NASH_RUNTIME_RUNTIME_H
#define _NASH_RUNTIME_RUNTIME_H

#include <uuid/uuid.h>
#include <sys/types.h>
#include "runtime/resources.h"

typedef struct {
    char *hostname;
    char *rootfs_path;
    char *exec_path;
    char **exec_argv;
    char **exec_envp;
    nsh_resources_t resources;
} nsh_runtime_config_t;

pid_t nsh_runtime_container_create(uuid_t id, nsh_runtime_config_t *config);
void nsh_runtime_container_cleanup(uuid_t id);

#endif

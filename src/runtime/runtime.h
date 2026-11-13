#ifndef _NASH_RUNTIME_RUNTIME_H
#define _NASH_RUNTIME_RUNTIME_H

#include <uuid/uuid.h>
#include <sys/types.h>
#include "runtime/resources.h"
#include "runtime/creds.h"

typedef struct {
    char *rootfs_path;
    char *hostname;
    char *cwd;
    char *exec_path;
    char **exec_argv;
    char **exec_envp;
    nsh_creds_t creds;
    nsh_resources_t resources;
} nsh_run_config_t;

pid_t nsh_run_create(uuid_t id, nsh_run_config_t *config);
void nsh_run_cleanup(uuid_t id);

#endif

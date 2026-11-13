#ifndef _NASH_RUNTIME_RUNTIME_H
#define _NASH_RUNTIME_RUNTIME_H

#include <uuid/uuid.h>
#include <sys/types.h>
#include "common/utils/macros.h"
#include "runtime/resources.h"
#include "runtime/creds.h"

typedef struct {
    char *hostname;
    char *workdir;
    ARRAY_WITH_LEN(char *, exec);
    ARRAY_WITH_LEN(char *, env);
    nsh_creds_t creds;
    nsh_resources_t resources;
} nsh_run_config_t;

typedef struct {
    char *rootfs_path;
    nsh_run_config_t config;
} nsh_run_params_t;

pid_t nsh_run_create(uuid_t id, nsh_run_params_t *params);
void nsh_run_cleanup(uuid_t id);

#endif

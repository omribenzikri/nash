#ifndef _NASH_ENGINE_CONTAINER_H
#define _NASH_ENGINE_CONTAINER_H

#include <sys/types.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include "runtime/runtime.h"
#include "engine/container/init.h"

typedef struct {
    uuid_t id;
    pid_t pid;
    char workdir_path[PATH_MAX];
    nsh_run_params_t run_params;
} nsh_container_t;

nsh_container_t *nsh_engine_container_create(nsh_container_params_t *params);
void nsh_engine_container_delete(nsh_container_t *container);
void nsh_engine_container_free(nsh_container_t *container);

#endif

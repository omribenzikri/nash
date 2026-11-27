#ifndef _NASH_ENGINE_CONTAINER_H
#define _NASH_ENGINE_CONTAINER_H

#include <sys/types.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include <container/common.h>
#include "engine/container/init.h"

typedef struct {
    uuid_t id;
    pid_t pid;
    cntr_config_t config;
} nsh_container_t;

nsh_container_t *nsh_container_create(nsh_container_params_t *params);
void nsh_container_delete(nsh_container_t *container);
void nsh_container_free(nsh_container_t *container);

#endif

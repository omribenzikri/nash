#ifndef _NASH_ENGINE_CONTAINER_INIT_H
#define _NASH_ENGINE_CONTAINER_INIT_H

#include <container/resources.h>
#include <container/common.h>
#include "engine/common/container_config.h"

typedef struct {
    char *image_name;
    char *hostname;
    nsh_container_config_t config;
    cntr_resources_t resources;
} nsh_container_params_t;

typedef struct {
    char *rootfs_path;
    nsh_container_params_t *user_params;
    nsh_container_config_t *image_config;
} nsh_init_context_t;

int nsh_run_config_init(cntr_config_t *config, nsh_init_context_t *ctx);
void nsh_run_config_destroy(cntr_config_t *config);

#endif

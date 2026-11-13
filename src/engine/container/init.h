#ifndef _NASH_ENGINE_CONTAINER_INIT_H
#define _NASH_ENGINE_CONTAINER_INIT_H

#include "runtime/runtime.h"
#include "engine/container/config/config.h"

typedef struct {
    char *image_path;
    char *hostname;
    nsh_container_config_t config;
    nsh_resources_t resources;
} nsh_container_params_t;

typedef struct {
    const char *rootfs_path;
    nsh_container_params_t *user_params;
    nsh_container_config_t *image_config;
} nsh_init_context_t;

int nsh_run_config_init(nsh_run_config_t *config, nsh_init_context_t *ctx);
void nsh_run_config_destroy(nsh_run_config_t *config);

int nsh_run_params_init(nsh_run_params_t *params, nsh_init_context_t *ctx);
void nsh_run_params_destroy(nsh_run_params_t *params);

#endif

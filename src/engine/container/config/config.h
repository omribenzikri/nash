#ifndef _NASH_ENGINE_CONTAINER_CONFIG_H
#define _NASH_ENGINE_CONTAINER_CONFIG_H

#include "runtime/runtime.h"
#include "runtime/resources.h"
#include "utils/macros.h"

#define NSH_DEFAULT_WORKING_DIRECTORY "/"
#define NSH_DEFAULT_USER "root"

// Not all image config parameters are here,
// just the ones that are currently supported.
typedef struct {
    char *user;
    char *working_dir;
    ARRAY_WITH_LEN(char *, env)
    ARRAY_WITH_LEN(char *, entrypoint)
    ARRAY_WITH_LEN(char *, cmd)
} nsh_container_config_t;

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
} nsh_runcfg_init_params_t;

int nsh_runtime_config_init(
    nsh_run_config_t *config,
    nsh_runcfg_init_params_t *params);

void nsh_runtime_config_destroy(nsh_run_config_t *config);

#endif

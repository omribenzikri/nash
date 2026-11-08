#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <linux/limits.h>
#include "runtime/runtime.h"
#include "engine/container/config.h"

typedef struct {
    nsh_container_params_t *user_params;
    nsh_container_config_t *image_config;
} nsh_runtime_config_sources_t;

static int nsh_runtime_config_init_rootfs_path(
    nsh_runtime_config_t *config, const char *rootfs_path)
{
    assert(rootfs_path != NULL);

    config->rootfs_path = strndup(rootfs_path, PATH_MAX);
    if (config->rootfs_path == NULL) {
        perror("[x] Failed to allocate rootfs path");
        return -1;
    }

    return 0;
}

static int nsh_runtime_config_init_hostname(
    nsh_runtime_config_t *config, nsh_container_params_t *params)
{
    if (params->hostname == NULL) {
        config->hostname = NULL;
        return 0;
    }

    config->hostname = strndup(params->hostname, PATH_MAX);
    if (config->hostname == NULL) {
        perror("[x] Failed to allocate hostname");
        return -1;
    }

    return 0;
}

static int nsh_runtime_config_init_working_directory(
    nsh_runtime_config_t *config, nsh_runtime_config_sources_t *sources)
{
    const char *wd;

    if (sources->user_params->config.working_dir != NULL) {
        wd = sources->user_params->config.working_dir;
    }
    else if (sources->image_config->working_dir != NULL) {
        wd = sources->image_config->working_dir;
    }
    else {
        wd = NSH_DEFAULT_WORKING_DIRECTORY;
    }

    config->working_directory = strndup(wd, PATH_MAX);
    if (config->working_directory == NULL) {
        perror("[x] Failed to allocate working directory");
        return -1;
    }

    return 0;
}

static int nsh_runtime_config_init_exec_path(
    nsh_runtime_config_t *config, nsh_container_params_t *params)
{
    assert(params->config.entrypoint[0] != NULL);

    config->exec_path = strndup(params->config.entrypoint[0], PATH_MAX);
    if (config->exec_path == NULL) {
        perror("[x] Failed to allocate exec path");
        return -1;
    }

    return 0;
}

static int nsh_runtime_config_init_exec_argv(
    nsh_runtime_config_t *config, nsh_container_params_t *params)
{
    char **exec_argv;
    size_t exec_argv_len;

    exec_argv_len = params->config.entrypoint_len +
                    params->config.cmd_len;

    exec_argv = calloc(exec_argv_len + 1, sizeof(*exec_argv));
    if (exec_argv == NULL) {
        perror("[x] Failed to allocate exec argv");
        return -1;
    }

    for (size_t i = 0; i < params->config.entrypoint_len; i++)
        exec_argv[i] = params->config.entrypoint[i];

    for (size_t i = 0; i < params->config.cmd_len; i++)
        exec_argv[i + params->config.entrypoint_len] = params->config.cmd[i];

    exec_argv[exec_argv_len] = NULL;
    config->exec_argv = exec_argv;

    return 0;
}

static int nsh_runtime_config_init_exec_envp(
    nsh_runtime_config_t *config, nsh_container_params_t *params)
{
    char **exec_envp;
    size_t exec_envp_len;

    exec_envp_len = params->config.env_len;

    exec_envp = calloc(exec_envp_len + 1, sizeof(*exec_envp));
    if (exec_envp == NULL) {
        perror("[x] Failed to allocate exec envp");
        return -1;
    }

    for (size_t i = 0; i < params->config.env_len; i++)
        exec_envp[i] = params->config.env[i];

    exec_envp[exec_envp_len] = NULL;
    config->exec_envp = exec_envp;

    return 0;
}

int nsh_runtime_config_init(
    nsh_runtime_config_t *config, nsh_container_params_t *params,
    nsh_container_config_t *image_config, const char *rootfs_path)
{
    nsh_runtime_config_sources_t sources;
    int err;

    sources.user_params = params;
    sources.image_config = image_config;

    err = nsh_runtime_config_init_rootfs_path(config, rootfs_path);
    if (err)
        goto error;

    err = nsh_runtime_config_init_hostname(config, params);
    if (err)
        goto error_free_rootfs_path;

    err = nsh_runtime_config_init_exec_path(config, params);
    if (err)
        goto error_free_hostname;

    err = nsh_runtime_config_init_exec_argv(config, params);
    if (err)
        goto error_free_exec_path;

    err = nsh_runtime_config_init_exec_envp(config, params);
    if (err)
        goto error_free_exec_argv;

    err = nsh_runtime_config_init_working_directory(config, &sources);
    if (err)
        goto error_free_exec_envp;

    return 0;

error_free_exec_envp:
    free(config->exec_envp);
error_free_exec_argv:
    free(config->exec_argv);
error_free_exec_path:
    free(config->exec_path);
error_free_hostname:
    free(config->hostname);
error_free_rootfs_path:
    free(config->rootfs_path);
error:
    return -1;
}

void nsh_runtime_config_destroy(nsh_runtime_config_t *config)
{
    free(config->working_directory);
    free(config->exec_envp);
    free(config->exec_argv);
    free(config->exec_path);
    free(config->hostname);
    free(config->rootfs_path);
}

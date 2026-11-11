#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <linux/limits.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "utils/array.h"
#include "utils/macros.h"
#include "runtime/runtime.h"
#include "engine/container/config.h"

typedef struct {
    nsh_container_params_t *user_params;
    nsh_container_config_t *image_config;
} nsh_runtime_config_sources_t;

static int nsh_runtime_config_rootfs_path_init(
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

static int nsh_runtime_config_hostname_init(
    nsh_runtime_config_t *config, nsh_runtime_config_sources_t *sources)
{
    if (sources->user_params->hostname == NULL) {
        config->hostname = NULL;
        return 0;
    }

    config->hostname = strndup(sources->user_params->hostname, PATH_MAX);
    if (config->hostname == NULL) {
        perror("[x] Failed to allocate hostname");
        return -1;
    }

    return 0;
}

static void nsh_runtime_config_cwd_pick(
    nsh_runtime_config_sources_t *sources, const char **out)
{
    if (sources->user_params->config.working_dir != NULL) {
        *out = sources->user_params->config.working_dir;
    }
    else if (sources->image_config->working_dir != NULL) {
        *out = sources->image_config->working_dir;
    }
    else {
        *out = NSH_DEFAULT_WORKING_DIRECTORY;
    }
}

static int nsh_runtime_config_cwd_init(
    nsh_runtime_config_t *config, nsh_runtime_config_sources_t *sources)
{
    const char *cwd;

    nsh_runtime_config_cwd_pick(sources, &cwd);

    config->cwd = strndup(cwd, PATH_MAX);
    if (config->cwd == NULL) {
        perror("[x] Failed to allocate cwd");
        return -1;
    }

    return 0;
}

static void nsh_runtime_config_entrypoint_pick(
    nsh_runtime_config_sources_t *sources, char ***outv, size_t *outc)
{
    if (sources->user_params->config.entrypoint_len > 0) {
        *outv = sources->user_params->config.entrypoint;
        *outc = sources->user_params->config.entrypoint_len;
    }
    else if (sources->image_config->entrypoint_len > 0) {
        *outv = sources->image_config->entrypoint;
        *outc = sources->image_config->entrypoint_len;
    }
    else {
        *outv = NULL;
        *outc = 0;
    }
}

static void nsh_runtime_config_cmd_pick(
    nsh_runtime_config_sources_t *sources, char ***outv, size_t *outc)
{
    if (sources->user_params->config.cmd_len > 0) {
        *outv = sources->user_params->config.cmd;
        *outc = sources->user_params->config.cmd_len;
    }
    else if (sources->image_config->cmd_len > 0) {
        *outv = sources->image_config->cmd;
        *outc = sources->image_config->cmd_len;
    }
    else {
        *outv = NULL;
        *outc = 0;
    }
}

static void nsh_runtime_config_exec_path_pick(
    nsh_runtime_config_sources_t *sources, const char **out)
{
    ARRAY_WITH_LEN(char *, entrypoint);
    ARRAY_WITH_LEN(char *, cmd);

    nsh_runtime_config_entrypoint_pick(sources, &entrypoint, &entrypoint_len);
    nsh_runtime_config_cmd_pick(sources, &cmd, &cmd_len);

    if (entrypoint == NULL)
        *out = cmd != NULL ? cmd[0] : NULL;
    else
        *out = entrypoint[0];
}

static int nsh_runtime_config_exec_path_init(
    nsh_runtime_config_t *config, nsh_runtime_config_sources_t *sources)
{
    const char *exec_path = NULL;

    nsh_runtime_config_exec_path_pick(sources, &exec_path);

    if (exec_path == NULL) {
        fprintf(stderr, "[x] Container executable path missing\n");
        return -1;
    }

    config->exec_path = strndup(exec_path, PATH_MAX);
    if (config->exec_path == NULL) {
        perror("[x] Failed to allocate exec path");
        return -1;
    }

    return 0;
}

static int nsh_runtime_config_exec_argv_init(
    nsh_runtime_config_t *config, nsh_runtime_config_sources_t *sources)
{
    ARRAY_WITH_LEN(char *, entrypoint);
    ARRAY_WITH_LEN(char *, cmd);
    struct cc_array_s *args;
    enum cc_stat cc_stat;

    cc_stat = cc_array_new(&args);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate args dynarray\n");
        goto error;
    }

    nsh_runtime_config_entrypoint_pick(sources, &entrypoint, &entrypoint_len);
    nsh_runtime_config_cmd_pick(sources, &cmd, &cmd_len);

    cc_stat = cc_array_extend_strings(args, entrypoint, entrypoint_len);
    if (cc_stat != CC_OK)
        goto error_destroy_args;

    cc_stat = cc_array_extend_strings(args, cmd, cmd_len);
    if (cc_stat != CC_OK)
        goto error_destroy_args;

    cc_stat = cc_array_to_static_shallow(args, (void ***)&config->exec_argv);
    if (cc_stat != CC_OK)
        goto error_destroy_args;

    cc_array_destroy(args);
    return 0;

error_destroy_args:
    cc_array_destroy_cb(args, free);
error:
    return -1;
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
    nsh_runtime_config_t *config, nsh_container_params_t *user_params,
    nsh_container_config_t *image_config, const char *rootfs_path)
{
    nsh_runtime_config_sources_t sources = {
        .user_params = user_params,
        .image_config = image_config
    };
    int err;

    err = nsh_runtime_config_rootfs_path_init(config, rootfs_path);
    if (err)
        goto error;

    err = nsh_runtime_config_hostname_init(config, &sources);
    if (err)
        goto error_free_rootfs_path;

    err = nsh_runtime_config_exec_path_init(config, &sources);
    if (err)
        goto error_free_hostname;

    err = nsh_runtime_config_exec_argv_init(config, &sources);
    if (err)
        goto error_free_exec_path;

    err = nsh_runtime_config_init_exec_envp(config, user_params);
    if (err)
        goto error_free_exec_argv;

    err = nsh_runtime_config_cwd_init(config, &sources);
    if (err)
        goto error_free_exec_envp;

    return 0;

error_free_exec_envp:
    free(config->exec_envp);
error_free_exec_argv:
    ARRAY_ITESM_FREE(config->exec_argv);
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
    free(config->cwd);
    free(config->exec_envp);
    free(config->exec_argv);
    free(config->exec_path);
    free(config->hostname);
    free(config->rootfs_path);
}

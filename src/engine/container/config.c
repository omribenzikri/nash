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
#include "runtime/creds.h"
#include "runtime/runtime.h"
#include "engine/container/creds.h"
#include "engine/container/config.h"

static bool nsh_env_is_valid(const char *var)
{
    char *delim;
    size_t delim_idx;

    delim = strchr(var, '=');
    if (delim == NULL)
        return false;

    delim_idx = delim - var;

    return  0 < delim_idx && delim_idx < strlen(var);
}

static bool nsh_env_cmp_key(const char *var1, const char *var2)
{
    char *delim1; size_t delim1_idx;
    char *delim2; size_t delim2_idx;
    size_t len;

    delim1 = strchr(var1, '=');
    delim2 = strchr(var2, '=');

    if (delim1 == NULL || delim2 == NULL)
        return false;

    delim1_idx = delim1 - var1;
    delim2_idx = delim2 - var2;
    len = (delim1_idx <= delim2_idx) ? delim1_idx : delim2_idx;

    return strncmp(var1, var2, len) == 0;
}

static int nsh_runtime_config_rootfs_path_init(
    char **rootfs_path, nsh_runcfg_init_params_t *params)
{
    assert(params->rootfs_path != NULL);

    *rootfs_path = strndup(params->rootfs_path, PATH_MAX);
    if (*rootfs_path == NULL) {
        perror("[x] Failed to allocate rootfs path");
        return -1;
    }

    return 0;
}

static int nsh_runtime_config_hostname_init(
    char **hostname, nsh_runcfg_init_params_t *params)
{
    if (params->user_params->hostname == NULL) {
        *hostname = NULL;
        return 0;
    }

    *hostname = strndup(params->user_params->hostname, PATH_MAX);
    if (*hostname == NULL) {
        perror("[x] Failed to allocate hostname");
        return -1;
    }

    return 0;
}

static void nsh_runtime_config_cwd_pick(
    nsh_runcfg_init_params_t *params, char **out)
{
    if (params->user_params->config.working_dir != NULL) {
        *out = params->user_params->config.working_dir;
    }
    else if (params->image_config->working_dir != NULL) {
        *out = params->image_config->working_dir;
    }
    else {
        *out = NSH_DEFAULT_WORKING_DIRECTORY;
    }
}

static int nsh_runtime_config_cwd_init(
    char **cwd, nsh_runcfg_init_params_t *params)
{
    char *cwd_temp;

    nsh_runtime_config_cwd_pick(params, &cwd_temp);

    *cwd = strndup(cwd_temp, PATH_MAX);
    if (*cwd == NULL) {
        perror("[x] Failed to allocate working directory");
        return -1;
    }

    return 0;
}

static void nsh_runtime_config_creds_str_pick(
    nsh_runcfg_init_params_t *params, char **out)
{
    if (params->user_params->config.user != NULL) {
        *out = params->user_params->config.user;
    }
    else if (params->image_config->user != NULL) {
        *out = params->image_config->user;
    }
    else {
        *out = NSH_DEFAULT_USER;
    }
}

static int nsh_runtime_config_creds_init(
    nsh_creds_t *creds, nsh_runcfg_init_params_t *params)
{
    char *creds_str;
    char *user;
    char *group;
    int err;
    int ret = 0;

    nsh_runtime_config_creds_str_pick(params, &creds_str);

    err = nsh_creds_parse(creds_str, &user, &group);
    if (err) {
        ret = -1;
        goto out;
    }

    err = nsh_creds_resolve(user, group, params->rootfs_path, creds);
    if (err) {
        ret = -1;
        goto out_free_user_group;
    }

out_free_user_group:
    if (user != NULL) free(user);
    if (group != NULL) free(group);
out:
    return ret;
}

static void nsh_runtime_config_creds_destroy(nsh_creds_t *creds)
{
    free(creds->sup_gids);
}

static void nsh_runtime_config_entrypoint_pick(
    nsh_runcfg_init_params_t *params, char ***outv, size_t *outc)
{
    if (params->user_params->config.entrypoint_len > 0) {
        *outv = params->user_params->config.entrypoint;
        *outc = params->user_params->config.entrypoint_len;
    }
    else if (params->image_config->entrypoint_len > 0) {
        *outv = params->image_config->entrypoint;
        *outc = params->image_config->entrypoint_len;
    }
    else {
        *outv = NULL;
        *outc = 0;
    }
}

static void nsh_runtime_config_cmd_pick(
    nsh_runcfg_init_params_t *params, char ***outv, size_t *outc)
{
    if (params->user_params->config.cmd_len > 0) {
        *outv = params->user_params->config.cmd;
        *outc = params->user_params->config.cmd_len;
    }
    else if (params->image_config->cmd_len > 0) {
        *outv = params->image_config->cmd;
        *outc = params->image_config->cmd_len;
    }
    else {
        *outv = NULL;
        *outc = 0;
    }
}

static void nsh_runtime_config_exec_path_pick(
    nsh_runcfg_init_params_t *params, char **out)
{
    ARRAY_WITH_LEN(char *, entrypoint);
    ARRAY_WITH_LEN(char *, cmd);

    nsh_runtime_config_entrypoint_pick(params, &entrypoint, &entrypoint_len);
    nsh_runtime_config_cmd_pick(params, &cmd, &cmd_len);

    if (entrypoint == NULL)
        *out = cmd != NULL ? cmd[0] : NULL;
    else
        *out = entrypoint[0];
}

static int nsh_runtime_config_exec_path_init(
    char **exec_path, nsh_runcfg_init_params_t *params)
{
    char *exec_path_temp = NULL;

    nsh_runtime_config_exec_path_pick(params, &exec_path_temp);
    if (exec_path_temp == NULL) {
        fprintf(stderr, "[x] Container executable path missing\n");
        return -1;
    }

    *exec_path = strndup(exec_path_temp, PATH_MAX);
    if (*exec_path == NULL) {
        perror("[x] Failed to allocate executable path");
        return -1;
    }

    return 0;
}

static int nsh_runtime_config_exec_argv_init(
    char ***exec_argv, nsh_runcfg_init_params_t *params)
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

    nsh_runtime_config_entrypoint_pick(params, &entrypoint, &entrypoint_len);
    nsh_runtime_config_cmd_pick(params, &cmd, &cmd_len);

    cc_stat = cc_array_extend_strings(args, entrypoint, entrypoint_len);
    if (cc_stat != CC_OK)
        goto error_destroy_args;

    cc_stat = cc_array_extend_strings(args, cmd, cmd_len);
    if (cc_stat != CC_OK)
        goto error_destroy_args;

    cc_stat = cc_array_to_vector_shallow(args, (void ***)exec_argv);
    if (cc_stat != CC_OK)
        goto error_destroy_args;

    cc_array_destroy(args);
    return 0;

error_destroy_args:
    cc_array_destroy_cb(args, free);
error:
    return -1;
}

static bool nsh_runtime_config_env_var_exists(
    struct cc_array_s *existing, const char *var)
{
    char *existing_var;

    for (size_t i = 0; i < cc_array_size(existing); i ++) {
        cc_array_get_at(existing, i, (void **)&existing_var);

        if (nsh_env_cmp_key(var, existing_var))
            return true;
    }

    return false;
}

static int nsh_runtime_config_env_apply_source(
    struct cc_array_s *vars, char **src, size_t src_len)
{
    char *var;
    enum cc_stat cc_stat;

    for (size_t i = 0; i < src_len; i++) {
        var = src[i];
        if (!nsh_env_is_valid(var)) {
            fprintf(stderr, "[x] Invalid environment variable: %s\n", var);
            return -1;
        }

        if (nsh_runtime_config_env_var_exists(vars, var))
            continue;

        cc_stat = cc_array_add_string(vars, var);
        if (cc_stat != CC_OK) {
            fprintf(stderr, "[x] Failed to add environment variable: %s\n", var);
            return -1;
        }
    }

    return 0;
}

static int nsh_runtime_config_exec_envp_init(
    char ***exec_envp, nsh_runcfg_init_params_t *params)
{
    struct cc_array_s *vars;
    enum cc_stat cc_stat;
    int err;

    cc_stat = cc_array_new(&vars);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate env dynarray");
        goto error;
    }

    err = nsh_runtime_config_env_apply_source(vars,
        params->user_params->config.env,
        params->user_params->config.env_len);
    if (err)
        goto error_destroy_env;

    err = nsh_runtime_config_env_apply_source(vars,
        params->image_config->env,
        params->image_config->env_len);
    if (err)
        goto error_destroy_env;

    cc_stat = cc_array_to_vector_shallow(vars, (void ***)exec_envp);
    if (cc_stat != CC_OK)
        goto error_destroy_env;

    cc_array_destroy(vars);

    return 0;

error_destroy_env:
    cc_array_destroy_cb(vars, free);
error:
    return -1;
}

int nsh_runtime_config_init(
    nsh_runtime_config_t *config,
    nsh_runcfg_init_params_t *params)
{
    int err;

    err = nsh_runtime_config_rootfs_path_init(&config->rootfs_path, params);
    if (err)
        goto error;

    err = nsh_runtime_config_hostname_init(&config->hostname, params);
    if (err)
        goto error_free_rootfs_path;

    err = nsh_runtime_config_exec_path_init(&config->exec_path, params);
    if (err)
        goto error_free_hostname;

    err = nsh_runtime_config_exec_argv_init(&config->exec_argv, params);
    if (err)
        goto error_free_exec_path;

    err = nsh_runtime_config_exec_envp_init(&config->exec_envp, params);
    if (err)
        goto error_free_exec_argv;

    err = nsh_runtime_config_cwd_init(&config->cwd, params);
    if (err)
        goto error_free_exec_envp;

    err = nsh_runtime_config_creds_init(&config->creds, params);
    if (err)
        goto error_free_exec_cwd;

    return 0;

error_free_exec_cwd:
    free(config->cwd);
error_free_exec_envp:
    ARRAY_FREE_DEEP(config->exec_envp);
error_free_exec_argv:
    ARRAY_FREE_DEEP(config->exec_argv);
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
    nsh_runtime_config_creds_destroy(&config->creds);

    free(config->cwd);
    ARRAY_FREE_DEEP(config->exec_envp);
    ARRAY_FREE_DEEP(config->exec_argv);
    free(config->exec_path);
    free(config->hostname);
    free(config->rootfs_path);
}

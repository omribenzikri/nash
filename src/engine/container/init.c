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
#include "engine/container/config/env.h"
#include "engine/container/config/creds.h"
#include "engine/container/config/config.h"
#include "engine/container/init.h"

static int nsh_run_params_rootfs_path_init(
    char **rootfs_path, nsh_init_context_t *ctx)
{
    assert(ctx->rootfs_path != NULL);

    *rootfs_path = strndup(ctx->rootfs_path, PATH_MAX);
    if (*rootfs_path == NULL) {
        perror("[x] Failed to allocate rootfs path");
        return -1;
    }

    return 0;
}

static int nsh_run_config_hostname_init(
    char **hostname, nsh_init_context_t *ctx)
{
    if (ctx->user_params->hostname == NULL) {
        *hostname = NULL;
        return 0;
    }

    *hostname = strndup(ctx->user_params->hostname, PATH_MAX);
    if (*hostname == NULL) {
        perror("[x] Failed to allocate hostname");
        return -1;
    }

    return 0;
}

static void nsh_run_config_cwd_pick(
    nsh_init_context_t *ctx, char **out)
{
    if (ctx->user_params->config.working_dir != NULL) {
        *out = ctx->user_params->config.working_dir;
    }
    else if (ctx->image_config->working_dir != NULL) {
        *out = ctx->image_config->working_dir;
    }
    else {
        *out = NSH_DEFAULT_WORKING_DIRECTORY;
    }
}

static int nsh_run_config_cwd_init(
    char **cwd, nsh_init_context_t *ctx)
{
    char *cwd_temp;

    nsh_run_config_cwd_pick(ctx, &cwd_temp);

    *cwd = strndup(cwd_temp, PATH_MAX);
    if (*cwd == NULL) {
        perror("[x] Failed to allocate working directory");
        return -1;
    }

    return 0;
}

static void nsh_run_config_creds_str_pick(
    nsh_init_context_t *ctx, char **out)
{
    if (ctx->user_params->config.user != NULL) {
        *out = ctx->user_params->config.user;
    }
    else if (ctx->image_config->user != NULL) {
        *out = ctx->image_config->user;
    }
    else {
        *out = NSH_DEFAULT_USER;
    }
}

static int nsh_run_config_creds_init(
    nsh_creds_t *creds, nsh_init_context_t *ctx)
{
    char *creds_str;
    char *user;
    char *group;
    int err;
    int ret = 0;

    nsh_run_config_creds_str_pick(ctx, &creds_str);

    err = nsh_creds_parse(creds_str, &user, &group);
    if (err) {
        ret = -1;
        goto out;
    }

    err = nsh_creds_resolve(user, group, ctx->rootfs_path, creds);
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

static void nsh_run_config_creds_destroy(nsh_creds_t *creds)
{
    free(creds->sup_gids);
}

static void nsh_run_config_entrypoint_pick(
    nsh_init_context_t *ctx, char ***outv, size_t *outc)
{
    if (ctx->user_params->config.entrypoint_len > 0) {
        *outv = ctx->user_params->config.entrypoint;
        *outc = ctx->user_params->config.entrypoint_len;
    }
    else if (ctx->image_config->entrypoint_len > 0) {
        *outv = ctx->image_config->entrypoint;
        *outc = ctx->image_config->entrypoint_len;
    }
    else {
        *outv = NULL;
        *outc = 0;
    }
}

static void nsh_run_config_cmd_pick(
    nsh_init_context_t *ctx, char ***outv, size_t *outc)
{
    if (ctx->user_params->config.cmd_len > 0) {
        *outv = ctx->user_params->config.cmd;
        *outc = ctx->user_params->config.cmd_len;
    }
    else if (ctx->image_config->cmd_len > 0) {
        *outv = ctx->image_config->cmd;
        *outc = ctx->image_config->cmd_len;
    }
    else {
        *outv = NULL;
        *outc = 0;
    }
}

static void nsh_run_config_exec_path_pick(
    nsh_init_context_t *ctx, char **out)
{
    ARRAY_WITH_LEN(char *, entrypoint);
    ARRAY_WITH_LEN(char *, cmd);

    nsh_run_config_entrypoint_pick(ctx, &entrypoint, &entrypoint_len);
    nsh_run_config_cmd_pick(ctx, &cmd, &cmd_len);

    if (entrypoint == NULL)
        *out = cmd != NULL ? cmd[0] : NULL;
    else
        *out = entrypoint[0];
}

static int nsh_run_config_exec_path_init(
    char **exec_path, nsh_init_context_t *ctx)
{
    char *exec_path_temp = NULL;

    nsh_run_config_exec_path_pick(ctx, &exec_path_temp);
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

static int nsh_run_config_exec_argv_init(
    char ***exec_argv, nsh_init_context_t *ctx)
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

    nsh_run_config_entrypoint_pick(ctx, &entrypoint, &entrypoint_len);
    nsh_run_config_cmd_pick(ctx, &cmd, &cmd_len);

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

static bool nsh_run_config_env_var_exists(
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

static int nsh_run_config_env_apply_source(
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

        if (nsh_run_config_env_var_exists(vars, var))
            continue;

        cc_stat = cc_array_add_string(vars, var);
        if (cc_stat != CC_OK) {
            fprintf(stderr, "[x] Failed to add environment variable: %s\n", var);
            return -1;
        }
    }

    return 0;
}

static int nsh_run_config_exec_envp_init(
    char ***exec_envp, nsh_init_context_t *ctx)
{
    struct cc_array_s *vars;
    enum cc_stat cc_stat;
    int err;

    cc_stat = cc_array_new(&vars);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate env dynarray");
        goto error;
    }

    err = nsh_run_config_env_apply_source(vars,
        ctx->user_params->config.env,
        ctx->user_params->config.env_len);
    if (err)
        goto error_destroy_env;

    err = nsh_run_config_env_apply_source(vars,
        ctx->image_config->env,
        ctx->image_config->env_len);
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

int nsh_run_config_init(nsh_run_config_t *config, nsh_init_context_t *ctx)
{
    int err;

    err = nsh_run_config_hostname_init(&config->hostname, ctx);
    if (err)
        goto error;

    err = nsh_run_config_exec_path_init(&config->exec_path, ctx);
    if (err)
        goto error_free_hostname;

    err = nsh_run_config_exec_argv_init(&config->exec_argv, ctx);
    if (err)
        goto error_free_exec_path;

    err = nsh_run_config_exec_envp_init(&config->exec_envp, ctx);
    if (err)
        goto error_free_exec_argv;

    err = nsh_run_config_cwd_init(&config->cwd, ctx);
    if (err)
        goto error_free_exec_envp;

    err = nsh_run_config_creds_init(&config->creds, ctx);
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
error:
    return -1;
}

void nsh_run_config_destroy(nsh_run_config_t *config)
{
    nsh_run_config_creds_destroy(&config->creds);

    free(config->cwd);
    ARRAY_FREE_DEEP(config->exec_envp);
    ARRAY_FREE_DEEP(config->exec_argv);
    free(config->exec_path);
    free(config->hostname);
}

int nsh_run_params_init(nsh_run_params_t *params, nsh_init_context_t *ctx)
{
    int err;

    err = nsh_run_params_rootfs_path_init(&params->rootfs_path, ctx);
    if (err)
        goto error;

    err = nsh_run_config_init(&params->config, ctx);
    if (err)
        goto error_free_rootfs_path;

    return 0;

error_free_rootfs_path:
    free(params->rootfs_path);
error:
    return -1;
}

void nsh_run_params_destroy(nsh_run_params_t *params)
{
    free(params->rootfs_path);
    nsh_run_config_destroy(&params->config);
}

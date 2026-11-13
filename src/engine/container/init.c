#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <linux/limits.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "common/utils/array.h"
#include "common/utils/macros.h"
#include "runtime/creds.h"
#include "runtime/runtime.h"
#include "engine/container/config/env.h"
#include "engine/container/config/creds.h"
#include "engine/container/config/config.h"
#include "engine/container/init.h"

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

static void nsh_run_config_workdir_pick(
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

static int nsh_run_config_workdir_init(
    char **cwd, nsh_init_context_t *ctx)
{
    char *cwd_temp;

    nsh_run_config_workdir_pick(ctx, &cwd_temp);

    *cwd = strndup(cwd_temp, PATH_MAX);
    if (*cwd == NULL) {
        perror("[x] Failed to allocate working directory");
        return -1;
    }

    return 0;
}

static void nsh_run_config_creds_pick(
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

    nsh_run_config_creds_pick(ctx, &creds_str);

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

static int nsh_run_config_exec_init(
    char ***exec, size_t *exec_len, nsh_init_context_t *ctx)
{
    ARRAY_WITH_LEN(char *, entrypoint);
    ARRAY_WITH_LEN(char *, cmd);
    struct cc_array_s *exec_temp;
    enum cc_stat cc_stat;

    cc_stat = cc_array_new(&exec_temp);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate exec dynarray\n");
        goto error;
    }

    nsh_run_config_entrypoint_pick(ctx, &entrypoint, &entrypoint_len);
    nsh_run_config_cmd_pick(ctx, &cmd, &cmd_len);

    cc_stat = cc_array_extend_strings(exec_temp, entrypoint, entrypoint_len);
    if (cc_stat != CC_OK)
        goto error_destroy_exec_temp;

    cc_stat = cc_array_extend_strings(exec_temp, cmd, cmd_len);
    if (cc_stat != CC_OK)
        goto error_destroy_exec_temp;

    cc_stat = cc_array_to_static_shallow(exec_temp, (void ***)exec, exec_len);
    if (cc_stat != CC_OK)
        goto error_destroy_exec_temp;

    cc_array_destroy(exec_temp);
    return 0;

error_destroy_exec_temp:
    cc_array_destroy_cb(exec_temp, free);
error:
    return -1;
}

static bool nsh_run_config_env_exists(
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

        if (nsh_run_config_env_exists(vars, var))
            continue;

        cc_stat = cc_array_add_string(vars, var);
        if (cc_stat != CC_OK) {
            fprintf(stderr, "[x] Failed to add environment variable: %s\n", var);
            return -1;
        }
    }

    return 0;
}

static int nsh_run_config_env_init(
    char ***env, size_t *env_len, nsh_init_context_t *ctx)
{
    struct cc_array_s *env_temp;
    enum cc_stat cc_stat;
    int err;

    cc_stat = cc_array_new(&env_temp);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate env dynarray");
        goto error;
    }

    err = nsh_run_config_env_apply_source(env_temp,
        ctx->user_params->config.env,
        ctx->user_params->config.env_len);
    if (err)
        goto error_destroy_env_temp;

    err = nsh_run_config_env_apply_source(env_temp,
        ctx->image_config->env,
        ctx->image_config->env_len);
    if (err)
        goto error_destroy_env_temp;

    cc_stat = cc_array_to_static_shallow(env_temp, (void ***)env, env_len);
    if (cc_stat != CC_OK)
        goto error_destroy_env_temp;

    cc_array_destroy(env_temp);

    return 0;

error_destroy_env_temp:
    cc_array_destroy_cb(env_temp, free);
error:
    return -1;
}

int nsh_run_config_init(nsh_run_config_t *config, nsh_init_context_t *ctx)
{
    int err;

    err = nsh_run_config_hostname_init(&config->hostname, ctx);
    if (err)
        goto error;

    err = nsh_run_config_workdir_init(&config->workdir, ctx);
    if (err)
        goto error_free_hostname;

    err = nsh_run_config_exec_init(&config->exec, &config->exec_len, ctx);
    if (err)
        goto error_free_workdir;

    err = nsh_run_config_env_init(&config->env, &config->env_len, ctx);
    if (err)
        goto error_free_exec;

    err = nsh_run_config_creds_init(&config->creds, ctx);
    if (err)
        goto error_free_env;

    return 0;


error_free_env:
    ARRAY_FREE_DEEP(config->env);
error_free_exec:
    ARRAY_FREE_DEEP(config->exec);
error_free_workdir:
    free(config->workdir);
error_free_hostname:
    free(config->hostname);
error:
    return -1;
}

void nsh_run_config_destroy(nsh_run_config_t *config)
{
    nsh_run_config_creds_destroy(&config->creds);
    ARRAY_FREE_DEEP(config->env);
    ARRAY_FREE_DEEP(config->exec);
    free(config->workdir);
    free(config->hostname);
}

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

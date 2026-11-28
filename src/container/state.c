#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include <json-c/json_object.h>
#include <json-c/json_types.h>
#include <json-c/json_util.h>
#include "container/state.h"
#include "container/common.h"

static inline void cntr_state_path_resolve(const char *workspace_path, char *buf, size_t size)
{
    snprintf(buf, size, "%s/state.json", workspace_path);
}

static json_object *cntr_state_config_creds_to_json(cntr_creds_t *creds)
{
    json_object *root;
    json_object *sup_gids;

    root = json_object_new_object();
    json_object_object_add(root, "uid", json_object_new_uint64(creds->uid));
    json_object_object_add(root, "gid", json_object_new_uint64(creds->gid));

    sup_gids = json_object_new_array();
    for (size_t i = 0; i < creds->sup_gids_len; i++)
        json_object_array_add(sup_gids, json_object_new_uint64(creds->sup_gids[i]));
    json_object_object_add(root, "sup_gids", sup_gids);

    return root;
}

static json_object *cntr_state_config_resources_to_json(cntr_resources_t *resources)
{
    json_object *root;

    root = json_object_new_object();

    json_object_object_add(root, "cpu_time", json_object_new_int(resources->cpu_time));
    json_object_object_add(root, "procs", json_object_new_int(resources->procs));
    json_object_object_add(root, "memory_bytes", json_object_new_int64(resources->memory_bytes));
    json_object_object_add(root, "memory_swap_bytes", json_object_new_int64(resources->memory_swap_bytes));

    return root;
}

static json_object *cntr_state_config_to_json(cntr_config_t *config)
{
    json_object *root;
    json_object *exec;
    json_object *env;

    root = json_object_new_object();

    json_object_object_add(root, "rootfs", json_object_new_string(config->rootfs_path));
    json_object_object_add(root, "hostname", json_object_new_string(config->hostname));
    json_object_object_add(root, "workdir", json_object_new_string(config->workdir));

    exec = json_object_new_array();
    for (size_t i = 0; i < config->exec_len; i++)
        json_object_array_add(exec, json_object_new_string(config->exec[i]));
    json_object_object_add(root, "exec", exec);

    env = json_object_new_array();
    for (size_t i = 0; i < config->env_len; i++)
        json_object_array_add(env, json_object_new_string(config->env[i]));
    json_object_object_add(root, "env", env);

    json_object_object_add(root, "creds", cntr_state_config_creds_to_json(&config->creds));
    json_object_object_add(root, "resources", cntr_state_config_resources_to_json(&config->resources));

    return root;
}

static json_object *cntr_state_to_json(cntr_state_t *state)
{
    char uuid_string[UUID_STR_LEN];
    json_object *root;

    uuid_unparse_lower(state->id, uuid_string);

    root = json_object_new_object();
    json_object_object_add(root, "id", json_object_new_string(uuid_string));
    json_object_object_add(root, "pid", json_object_new_int(state->pid));
    json_object_object_add(root, "start_time", json_object_new_uint64(state->start_time));
    json_object_object_add(root, "config", cntr_state_config_to_json(&state->config));

    return root;
}

int cntr_state_dump(cntr_state_t *state, const char *workspace_path)
{
    char path[PATH_MAX];
    json_object *json;
    int err;

    cntr_state_path_resolve(workspace_path, path, sizeof(path));

    json = cntr_state_to_json(state);

    err = json_object_to_file_ext(path, json,
        JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    if (err) {
        fprintf(stderr, "[x] Failed to dump state: %s",
            json_util_get_last_err());
        return -1;
    }

    return 0;
}

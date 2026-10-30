#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include "runtime/runtime.h"
#include "container.h"

#define NSH_GEN_HOSTNAME_LEN 13

char *nsh_generate_hostname(uuid_t container_id)
{
    char uuid_string[UUID_STR_LEN];
    char *hostname = uuid_string + sizeof(uuid_string)
                                 - NSH_GEN_HOSTNAME_LEN;

    assert(NSH_GEN_HOSTNAME_LEN < UUID_STR_LEN);
    uuid_unparse_lower(container_id, uuid_string);

    return strndup(hostname, NSH_GEN_HOSTNAME_LEN);
}

int nsh_runtime_config_init(nsh_runtime_config_t *runtime_config,
                            nsh_container_config_t *user_config,
                            uuid_t container_id)
{
    runtime_config->hostname = user_config->hostname == NULL
        ? nsh_generate_hostname(container_id)
        : strndup(user_config->hostname, HOST_NAME_MAX);

    if (runtime_config->hostname == NULL) {
        fprintf(stderr, "[x] Failed to allocate container hostname\n");
        return -1;
    }

    runtime_config->rootfs_path = user_config->image_path;
    runtime_config->exec_path = user_config->exec_path;
    runtime_config->exec_argv = user_config->exec_argv;
    runtime_config->exec_envp = user_config->exec_envp;
    runtime_config->resources = user_config->resources;

    return 0;
}

void nsh_runtime_config_destroy(nsh_runtime_config_t *config)
{
    free(config->hostname);
}

int nsh_container_create_internal(uuid_t id,
    nsh_container_config_t *user_config, nsh_container_t *out)
{
    nsh_runtime_config_t runtime_config;
    pid_t pid;
    int ret = 0;
    int err;

    err = nsh_runtime_config_init(&runtime_config, user_config, id);
    if (err) {
        ret = -1;
        goto out;
    }

    pid = nsh_runtime_container_create(id, &runtime_config);
    if (pid == -1) {
        ret = -1;
        goto out_free_config;
    }

    out->pid = pid;

out_free_config:
    nsh_runtime_config_destroy(&runtime_config);
out:
    return ret;
}

int nsh_container_create(nsh_container_config_t *config, nsh_container_t *out)
{
    int err;

    uuid_generate_random(out->id);

    err = nsh_container_create_internal(out->id, config, out);
    if (err)
        return -1;

    return 0;
}

void nsh_container_delete(nsh_container_t *container)
{
    nsh_runtime_container_cleanup(container->id);
}

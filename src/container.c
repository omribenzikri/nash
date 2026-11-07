#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include "image/image.h"
#include "runtime/runtime.h"
#include "common/constants.h"
#include "utils/filesystem.h"
#include "container.h"

#define NSH_GEN_HOSTNAME_LEN 13

static void nsh_container_workdir_resolve_path(nsh_container_t *container)
{
    char uuid_string[UUID_STR_LEN];
    uuid_unparse_lower(container->id, uuid_string);

    snprintf(container->workdir_path, sizeof(container->workdir_path),
        "%s/%s", NSH_WORKDIR_CONTAINERS, uuid_string);
}

static int nsh_container_workspace_create(nsh_container_t *container)
{
    int err;

    nsh_container_workdir_resolve_path(container);

    err = mkdir_if_missing(container->workdir_path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create container working directory");
        return -1;
    }

    return 0;
}

static int nsh_container_workspace_delete(nsh_container_t *container)
{
    int err;

    err = rmdir_recursive(container->workdir_path);
    if (err) {
        perror("[x] Failed to delete container working directory");
        return -1;
    }

    return 0;
}

static char *nsh_generate_hostname(uuid_t container_id)
{
    char uuid_string[UUID_STR_LEN];
    char *hostname = uuid_string + sizeof(uuid_string)
                                 - NSH_GEN_HOSTNAME_LEN;

    assert(NSH_GEN_HOSTNAME_LEN < UUID_STR_LEN);
    uuid_unparse_lower(container_id, uuid_string);

    return strndup(hostname, NSH_GEN_HOSTNAME_LEN);
}

static int nsh_runtime_config_init(nsh_container_t *container,
                                   nsh_container_config_t *user_config)
{
    nsh_runtime_config_t *runtime_config =
        &container->runtime_config;

    runtime_config->hostname = user_config->hostname == NULL
        ? nsh_generate_hostname(container->id)
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

static void nsh_runtime_config_destroy(nsh_runtime_config_t *config)
{
    free(config->hostname);
}

static int nsh_container_init_rootfs(nsh_container_t *container, nsh_image_t *image)
{
    int err;
    char rootfs_path[PATH_MAX];

    err = nsh_image_rootfs_prepare(image, container->workdir_path,
                                   rootfs_path, sizeof(rootfs_path));
    if (err) {
        fprintf(stderr, "[x] Failed to prepare container rootfs\n");
        return -1;
    }

    return 0;
}

static int nsh_container_init_image_related(nsh_container_t *container, const char *image_path)
{
    nsh_image_t *image;
    int ret = 0;
    int err;

    image = nsh_image_load(image_path);
    if (image == NULL) {
        fprintf(stderr, "[x] Failed to load image\n");
        ret = -1;
        goto out;
    }

    err = nsh_container_init_rootfs(container, image);
    if (err) {
        ret = -1;
        goto out_free_image;
    }

out_free_image:
    nsh_image_free(image);
out:
    return ret;
}

static int nsh_container_init(nsh_container_t *container,
    nsh_container_config_t *user_config)
{
    int err;

    err = nsh_container_workspace_create(container);
    if (err)
        goto error;

    err = nsh_runtime_config_init(container, user_config);
    if (err)
        goto error_delete_workspace;

    // Temporary image location
    err = nsh_container_init_image_related(container, "images/oci/alpine");
    if (err)
        goto error_free_config;

    return 0;

error_free_config:
    nsh_runtime_config_destroy(&container->runtime_config);
error_delete_workspace:
    nsh_container_workspace_delete(container);
error:
    return -1;
}

static void nsh_container_destroy(nsh_container_t *container)
{
    nsh_runtime_config_destroy(&container->runtime_config);
    nsh_container_workspace_delete(container);
}

int nsh_container_create(nsh_container_config_t *config, nsh_container_t *out)
{
    int err;

    memset(out, 0, sizeof(*out));
    uuid_generate_random(out->id);

    err = nsh_container_init(out, config);
    if (err)
        goto error;

    out->pid = nsh_runtime_container_create(out->id, &out->runtime_config);
    if (out->pid == -1)
        goto error_destroy_container;

    return 0;

error_destroy_container:
    nsh_container_destroy(out);
error:
    return -1;
}

void nsh_container_delete(nsh_container_t *container)
{
    nsh_runtime_container_cleanup(container->id);
    nsh_container_destroy(container);
}

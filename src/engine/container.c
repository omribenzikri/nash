#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include "image/image.h"
#include "runtime/runtime.h"
#include "common/constants.h"
#include "utils/filesystem.h"
#include "engine/container/config/config.h"
#include "engine/container/init.h"
#include "engine/container.h"

static void nsh_resolve_container_workdir_path(
    uuid_t container_id, char *buf, size_t size)
{
    char uuid_string[UUID_STR_LEN];
    uuid_unparse_lower(container_id, uuid_string);

    snprintf(buf, size, "%s/%s", NSH_WORKDIR_CONTAINERS, uuid_string);
}

static void nsh_load_image_config(
    nsh_image_t *image, nsh_container_config_t *config)
{
    config->env = nsh_image_config_env(image);
    config->env_len = nsh_image_config_env_len(image);
    config->entrypoint = nsh_image_config_entrypoint(image);
    config->entrypoint_len = nsh_image_config_entrypoint_len(image);
    config->cmd = nsh_image_config_cmd(image);
    config->cmd_len = nsh_image_config_cmd_len(image);
    config->working_dir = nsh_image_config_working_dir(image);
    config->user = nsh_image_config_user(image);
}

static int nsh_container_init_internal(
    nsh_container_t *container, nsh_image_t *image,
    nsh_container_params_t *user_params)
{
    char rootfs_path[PATH_MAX];
    nsh_container_config_t image_config = {0};
    nsh_init_context_t ctx = {
        .rootfs_path = rootfs_path,
        .user_params = user_params,
        .image_config = &image_config
    };
    int err;

    uuid_generate_random(container->id);

    nsh_resolve_container_workdir_path(container->id,
        container->workdir_path, sizeof(container->workdir_path));

    err = mkdir(container->workdir_path, S_IRWXU);
    if (err)
        goto error;

    err = nsh_image_rootfs_prepare(image, container->workdir_path,
        rootfs_path, sizeof(rootfs_path));
    if (err)
        goto error_delete_workdir;

    nsh_load_image_config(image, &image_config);

    err = nsh_run_params_init(&container->run_params, &ctx);
    if (err)
        goto error_destroy_rootfs;

    return 0;

error_destroy_rootfs:
    nsh_image_rootfs_destroy(rootfs_path);
error_delete_workdir:
    rmdir_recursive(container->workdir_path);
error:
    return -1;
}

static int nsh_container_init(
    nsh_container_t *container, nsh_container_params_t *params)
{
    nsh_image_t *image;
    int ret = 0;
    int err;

    image = nsh_image_load(params->image_path);
    if (image == NULL) {
        fprintf(stderr, "[x] Failed to load image\n");
        ret = -1;
        goto out;
    }

    err = nsh_container_init_internal(container, image, params);
    if (err) {
        ret = -1;
        goto out_free_image;
    }

out_free_image:
    nsh_image_free(image);
out:
    return ret;
}

static void nsh_container_cleanup(nsh_container_t *container)
{
    nsh_image_rootfs_destroy(container->run_params.rootfs_path);
    rmdir_recursive(container->workdir_path);
}

static void nsh_container_destroy(nsh_container_t *container)
{
    nsh_run_params_destroy(&container->run_params);
}

nsh_container_t *nsh_engine_container_create(nsh_container_params_t *params)
{
    nsh_container_t *container;
    int err;

    container = calloc(1, sizeof(*container));
    if (container == NULL) {
        perror("[x] Failed to allocate container");
        goto error;
    }

    err = nsh_container_init(container, params);
    if (err)
        goto error_free_container;

    container->pid = nsh_run_create(container->id, &container->run_params);
    if (container->pid == -1)
        goto error_cleanup_destroy_container;

    return container;

error_cleanup_destroy_container:
    nsh_container_cleanup(container);
    nsh_container_destroy(container);
error_free_container:
    free(container);
error:
    return NULL;
}

void nsh_engine_container_delete(nsh_container_t *container)
{
    nsh_run_cleanup(container->id);
    nsh_container_cleanup(container);
}

void nsh_engine_container_free(nsh_container_t *container)
{
    nsh_container_destroy(container);
    free(container);
}

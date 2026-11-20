#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <linux/limits.h>
#include "common/constants.h"
#include "common/entities/image.h"
#include "common/utils/macros.h"
#include "engine/common/container_config.h"
#include "engine/persistence/database.h"
#include "engine/persistence/repos/images.h"
#include "engine/image/oci/common.h"
#include "engine/image/oci/store.h"
#include "engine/image/oci/config.h"
#include "engine/image/overlay.h"
#include "engine/image/image.h"

struct nsh_image {
    nsh_image_entity_t *entity;
    nsh_oci_image_meta_t oci_meta;
};

static nsh_image_entity_t *nsh_image_load_entity(const char *name)
{
    nsh_database_t *db;
    nsh_image_entity_t *entity = NULL;
    int err;

    db = nsh_database_open();
    if (db == NULL)
        goto out;

    err = nsh_images_repo_first_by_name(db, name, &entity);
    if (err)
        goto out_close_db;
    if (entity == NULL) {
        fprintf(stderr, "[x] Failed to load image: %s: image does not exist\n", name);
        goto out_close_db;
    }

out_close_db:
    nsh_database_close(db);
out:
    return entity;
}

nsh_image_t *nsh_image_load(const char *name)
{
    nsh_image_t *image;
    int err;

    image = malloc(sizeof(*image));
    if (image == NULL) {
        perror("[x] Failed to allocate image");
        goto error;
    }

    image->entity = nsh_image_load_entity(name);
    if (image->entity == NULL)
        goto error_free_image;

    err = nsh_oci_store_load_image_meta(
        &image->oci_meta, NSH_WORKDIR_IMAGES, image->entity->ref);
    if (err)
        goto error_free_entity;

    return image;

error_free_entity:
    nsh_image_entity_free(image->entity);
error_free_image:
    free(image);
error:
    return NULL;
}

void nsh_image_free(nsh_image_t *image)
{
    nsh_oci_image_meta_destroy(&image->oci_meta);
    nsh_image_entity_free(image->entity);
    free(image);
}

int nsh_image_rootfs_prepare(
    nsh_image_t *image, const char *workspace, const char *path)
{
    ARRAY_WITH_LEN(char *, snapshot_paths);
    int err;
    int ret = 0;

    err = mkdir(path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create rootfs directory");
        ret = -1;
        goto out;
    }

    err = nsh_oci_store_snapshot_paths(
        &image->oci_meta, NSH_WORKDIR_IMAGES,
        &snapshot_paths, &snapshot_paths_len);
    if (err) {
        ret = -1;
        goto out;
    }

    err = nsh_overlay_create(path, workspace, snapshot_paths, snapshot_paths_len);
    if (err) {
        fprintf(stderr, "[x] Failed to create rootfs overlay\n");
        ret = -1;
        goto out_free_paths;
    }

out_free_paths:
    ARRAY_FREE_DEEP(snapshot_paths);
out:
    return ret;
}

int nsh_image_rootfs_destroy(const char *path)
{
    int err;

    err = umount(path);
    if (err) {
        perror("[x] Failed to unmount container rootfs");
        return -1;
    }

    return 0;
}

void nsh_image_config_load(nsh_image_t *image, nsh_container_config_t *config)
{
    nsh_oci_config_load(&image->oci_meta, config);
}

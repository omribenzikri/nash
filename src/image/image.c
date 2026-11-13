#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <linux/limits.h>
#include "common/constants.h"
#include "image/oci.h"
#include "image/layer.h"
#include "image/overlay.h"
#include "image/image.h"

struct nsh_image {
    char *path;
    oci_image_t oci;
};

nsh_image_t *nsh_image_load(const char *path)
{
    nsh_image_t *image;
    int err;

    image = malloc(sizeof(*image));
    if (image == NULL) {
        fprintf(stderr, "[x] Failed to allocate image\n");
        goto error;
    }

    image->path = strndup(path, PATH_MAX);
    if (image->path == NULL) {
        fprintf(stderr, "[x] Failed to duplicate image path\n");
        goto error_free_image;
    }

    err = nsh_oci_image_load(&image->oci, path);
    if (err)
        goto error_free_path;

    return image;

error_free_path:
    free(image->path);
error_free_image:
    free(image);
error:
    return NULL;
}

void nsh_image_free(nsh_image_t *image)
{
    nsh_oci_image_free(&image->oci);
    free(image->path);
    free(image);
}

static int nsh_filesystem_workspace_create(int workdir_fd)
{
     const char *subdirs[] = {
        NSH_FS_WORKDIR,
        NSH_FS_WORKDIR_OVERLAY,
        NSH_FS_WORKDIR_OVERLAY_SNAPSHOTS,
        NSH_FS_WORKDIR_OVERLAY_UPPER,
        NSH_FS_WORKDIR_OVERLAY_WORKDIR,
        NSH_FS_WORKDIR_ROOTFS
    };
    const size_t subdirs_len = sizeof(subdirs) / sizeof(subdirs[0]);

    int err;

    for (size_t i = 0; i < subdirs_len; i++) {
        err = mkdirat(workdir_fd, subdirs[i], S_IRWXU);
        if (err) {
            perror("[x] Failed to create directory in workdir");
            return -1;
        }
    }

    return 0;
}

int nsh_image_rootfs_prepare(
    nsh_image_t *image, const char *workdir_path,
    char *path, size_t size)
{
    int workdir_fd;
    int ret = 0;
    int err;

    workdir_fd = open(workdir_path, O_RDONLY | O_DIRECTORY);
    if (workdir_fd == -1) {
        perror("[x] Failed to open container workdir");
        ret = -1;
        goto out;
    }

    err = nsh_filesystem_workspace_create(workdir_fd);
    if (err) {
        ret = -1;
        goto out_close_workdir;
    }

    err = nsh_layers_extract(image->oci.manifest, image->path, workdir_fd);
    if (err) {
        ret = -1;
        goto out_close_workdir;
    }

    err = nsh_overlay_create(image->oci.manifest, workdir_path, path, size);
    if (err) {
        ret = -1;
        goto out_close_workdir;
    }

out_close_workdir:
    err = close(workdir_fd);
    if (err) {
        perror("[x] Failed to close container workdir");
        ret = -1;
    }
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

char *nsh_image_config_working_dir(nsh_image_t *image)
{
    return image->oci.config->config->working_dir;
}

char *nsh_image_config_user(nsh_image_t *image)
{
    return image->oci.config->config->user;
}

char **nsh_image_config_entrypoint(nsh_image_t *image)
{
    return image->oci.config->config->entrypoint;
}

size_t nsh_image_config_entrypoint_len(nsh_image_t *image)
{
    return image->oci.config->config->entrypoint_len;
}

char **nsh_image_config_cmd(nsh_image_t *image)
{
    return image->oci.config->config->cmd;
}

size_t nsh_image_config_cmd_len(nsh_image_t *image)
{
    return image->oci.config->config->cmd_len;
}

char **nsh_image_config_env(nsh_image_t *image)
{
    return image->oci.config->config->env;
}

size_t nsh_image_config_env_len(nsh_image_t *image)
{
    return image->oci.config->config->env_len;
}

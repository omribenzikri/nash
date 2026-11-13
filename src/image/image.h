#ifndef _NASH_IMAGE_IMAGE_H
#define _NASH_IMAGE_IMAGE_H

#include <stddef.h>

typedef struct nsh_image nsh_image_t;

nsh_image_t *nsh_image_load(const char *path);
void nsh_image_free(nsh_image_t *image);

int nsh_image_rootfs_prepare(
    nsh_image_t *image, const char *workdir_path,
    char *path, size_t size);
int nsh_image_rootfs_destroy(const char *path);

char *nsh_image_config_user(nsh_image_t *image);
char *nsh_image_config_working_dir(nsh_image_t *image);
char **nsh_image_config_entrypoint(nsh_image_t *image);
size_t nsh_image_config_entrypoint_len(nsh_image_t *image);
char **nsh_image_config_cmd(nsh_image_t *image);
size_t nsh_image_config_cmd_len(nsh_image_t *image);
char **nsh_image_config_env(nsh_image_t *image);
size_t nsh_image_config_env_len(nsh_image_t *image);

#endif

#ifndef _NASH_ENGINE_IMAGE_IMAGE_H
#define _NASH_ENGINE_IMAGE_IMAGE_H

#include <stddef.h>
#include "engine/common/container_config.h"

typedef struct nsh_image nsh_image_t;

nsh_image_t *nsh_image_load(const char *name);
void nsh_image_free(nsh_image_t *image);

int nsh_image_rootfs_prepare(nsh_image_t *image, const char *workspace, const char *path);
int nsh_image_rootfs_destroy(const char *path);

void nsh_image_config_load(nsh_image_t *image, nsh_container_config_t *config);

#endif

#ifndef _NASH_IMAGE_IMAGE_H
#define _NASH_IMAGE_IMAGE_H

#include <stddef.h>

typedef struct nsh_image nsh_image_t;

nsh_image_t *nsh_image_load(const char *path);
void nsh_image_free(nsh_image_t *image);

int nsh_image_rootfs_prepare(
    nsh_image_t *image, const char *workdir_path,
    char *path, size_t size);

#endif

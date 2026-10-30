#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/limits.h>
#include "image/oci.h"
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

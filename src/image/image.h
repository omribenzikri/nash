#ifndef _NASH_IMAGE_IMAGE_H
#define _NASH_IMAGE_IMAGE_H

typedef struct nsh_image nsh_image_t;

nsh_image_t *nsh_image_load(const char *path);
void nsh_image_free(nsh_image_t *image);

#endif

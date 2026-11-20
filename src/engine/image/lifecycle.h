#ifndef _NASH_ENGINE_IMAGE_LIFECYCLE_H
#define _NASH_ENGINE_IMAGE_LIFECYCLE_H

#include <stddef.h>
#include <collectc/cc_array.h>

int nsh_image_import(const char *path, const char *name);
int nsh_image_list(struct cc_array_s **out);
int nsh_image_delete(const char *name);

#endif

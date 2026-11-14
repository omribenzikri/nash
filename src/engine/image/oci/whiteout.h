#ifndef _NASH_ENGINE_IMAGE_OCI_WHITEOUT_H
#define _NASH_ENGINE_IMAGE_OCI_WHITEOUT_H

#include <stdbool.h>
#include <stddef.h>

bool nsh_oci_is_whiteout_new(const char *path);
bool nsh_oci_is_opaque_whiteout_new(const char *path);
bool nsh_oci_is_valid_whiteout_new(const char *path);
int nsh_oci_whiteout_to_file_new(const char *path, char *buf, size_t size);

#endif

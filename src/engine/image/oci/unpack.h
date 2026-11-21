#ifndef _NASH_ENGINE_IMAGE_OCI_UNPACK_H
#define _NASH_ENGINE_IMAGE_OCI_UNPACK_H

#include <stdint.h>

/* Unpacks an OCI image layer archive at <path>, of format <format>
 * with optional filter <filter> to a destination directory
 * at <target>.
 * On success, returns the size of the uncompressed data. */
int64_t nsh_oci_layer_unpack(const char *path, const char *target, int format, int filter);

#endif

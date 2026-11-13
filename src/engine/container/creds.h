#ifndef _NASH_ENGINE_CONTAINER_CREDS_H
#define _NASH_ENGINE_CONTAINER_CREDS_H

#include <sys/types.h>
#include <collectc/cc_array.h>
#include "runtime/creds.h"

int nsh_creds_parse(const char *str, char **out_user, char **out_group);

int nsh_creds_resolve(const char *user, const char *group,
    const char *rootfs_path, nsh_creds_t *out);

#endif

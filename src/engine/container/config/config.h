#ifndef _NASH_ENGINE_CONTAINER_CONFIG_CONFIG_H
#define _NASH_ENGINE_CONTAINER_CONFIG_CONFIG_H

#include <stddef.h>
#include "utils/macros.h"

#define NSH_DEFAULT_WORKING_DIRECTORY "/"
#define NSH_DEFAULT_USER "root"

// Not all image config parameters are here,
// just the ones that are currently supported.
typedef struct {
    char *user;
    char *working_dir;
    ARRAY_WITH_LEN(char *, env)
    ARRAY_WITH_LEN(char *, entrypoint)
    ARRAY_WITH_LEN(char *, cmd)
} nsh_container_config_t;

#endif

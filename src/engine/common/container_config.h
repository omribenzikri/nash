#ifndef _NASH_ENGINE_COMMON_MODELS_H
#define _NASH_ENGINE_COMMON_MODELS_H

#include <stddef.h>
#include "utils/macros.h"

// Not all image config parameters are here,
// just the ones that are currently supported.
typedef struct {
    char *user;
    char *working_dir;
    ARRAY(char *, env)
    ARRAY(char *, entrypoint)
    ARRAY(char *, cmd)
} nsh_container_config_t;

#endif

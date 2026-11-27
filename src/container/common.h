#ifndef _NASH_CONTAINER_COMMON_H
#define _NASH_CONTAINER_COMMON_H

#include "utils/macros.h"
#include "container/creds.h"
#include "container/resources.h"

typedef struct {
    char *rootfs_path;
    char *hostname;
    char *workdir;
    ARRAY(char *, exec);
    ARRAY(char *, env);
    cntr_creds_t creds;
    cntr_resources_t resources;
} cntr_config_t;

#endif

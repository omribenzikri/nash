#ifndef _NASH_CONTAINER_CREDS_H
#define _NASH_CONTAINER_CREDS_H

#include <sys/types.h>
#include "utils/macros.h"

typedef struct {
    uid_t uid;
    gid_t gid;
    ARRAY(gid_t, sup_gids);
} cntr_creds_t;

int cntr_creds_set(cntr_creds_t *creds);

#endif

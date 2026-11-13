#ifndef _NASH_RUNTIME_CREDS_H
#define _NASH_RUNTIME_CREDS_H

#include <stddef.h>
#include <sys/types.h>
#include "common/utils/macros.h"

typedef struct {
    uid_t uid;
    gid_t gid;
    ARRAY_WITH_LEN(gid_t, sup_gids);
} nsh_creds_t;

int nsh_creds_set(nsh_creds_t *creds);

#endif

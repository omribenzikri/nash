#ifndef _NASH_RUNTIME_CREDS_H
#define _NASH_RUNTIME_CREDS_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    uid_t uid;
    gid_t gid;
    gid_t *sup_gids;
    size_t sup_gids_len;
} nsh_creds_t;

int nsh_creds_set(nsh_creds_t *creds);

#endif

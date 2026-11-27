#include <stdio.h>
#include <unistd.h>
#include <grp.h>
#include "container/creds.h"

int cntr_creds_set(cntr_creds_t *creds)
{
    int err;

    err = setgroups(creds->sup_gids_len, creds->sup_gids);
    if (err) {
        perror("[x] Failed to set supplementary groups");
        return -1;
    }

    err = setgid(creds->gid);
    if (err) {
        perror("[x] Failed to set GID");
        return -1;
    }

    err = setuid(creds->uid);
    if (err) {
        perror("[x] Failed to set UID");
        return -1;
    }

    return 0;
}

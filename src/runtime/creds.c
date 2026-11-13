#include <stdio.h>
#include <unistd.h>
#include <grp.h>
#include "runtime/creds.h"

int nsh_creds_set(nsh_creds_t *creds)
{
    int err;

    err = setgroups(creds->sup_gids_len, creds->sup_gids);
    if (err) {
        perror("[x] Failed to set container supplementary groups");
        return -1;
    }

    err = setgid(creds->gid);
    if (err) {
        perror("[x] Failed to set container GID");
        return -1;
    }

    err = setuid(creds->uid);
    if (err) {
        perror("[x] Failed to set container UID");
        return -1;
    }

    return 0;
}

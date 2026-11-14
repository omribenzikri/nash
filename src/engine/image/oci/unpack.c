#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include "engine/image/oci/unpack.h"

int nsh_oci_layer_unpack(const char *path, const char *target, int format, int filter)
{
    char *cwd;
    int err;
    int ret = 0;

    err = mkdir(target, S_IRWXU);
    if (err) {
        perror("[x] Failed to create unpack target directory");
        ret = -1;
        goto out;
    }

    cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        perror("[x] Failed to retrieve current working directory");
        ret = -1;
        goto out;
    }

    err = chdir(target);
    if (err) {
        perror("[x] Failed to switch current working directory");
        ret = -1;
        goto out;
    }

    (void)path;
    (void)target;
    (void)format;
    (void)filter;

    goto out_restore_cwd;

out_restore_cwd:
    err = chdir(cwd);
    if (err) {
        perror("[x] Failed to restore current working directory");
        ret = -1;
    }
    free(cwd);
out:
    return ret;
}

#include <stdio.h>
#include <sys/stat.h>
#include "utils/file.h"
#include "engine/common/constants.h"
#include "engine/persistence/database.h"
#include "engine/startup.h"

static int nsh_engine_workdir_create()
{
    int err;

    err = mkdir_missing(NSH_WORKDIR, S_IRWXU);
    if (err) {
        perror("[x] Failed to create " NSH_WORKDIR);
        return -1;
    }

    err = mkdir_missing(NSH_WORKDIR_CONTAINERS, S_IRWXU);
    if (err) {
        perror("[x] Failed to create " NSH_WORKDIR_CONTAINERS);
        return -1;
    }

    err = mkdir_missing(NSH_WORKDIR_IMAGES, S_IRWXU);
    if (err) {
        perror("[x] Failed to create " NSH_WORKDIR_IMAGES);
        return -1;
    }

    return 0;
}

int nsh_engine_startup()
{
    int err;

    err = nsh_engine_workdir_create();
    if (err) {
        fprintf(stderr, "[x] Failed to create engine workdir\n");
        return -1;
    }

    err = nsh_database_create();
    if (err) {
        fprintf(stderr, "[x] Failed to initialize the engine database\n");
        return -1;
    }

    return 0;
}

#include <stdio.h>
#include <sys/stat.h>
#include <container/container.h>
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

static int nsh_engine_tempdir_create()
{
    int err;

    err = mkdir_missing(NSH_TEMPDIR, S_IRWXU);
    if (err) {
        perror("[x] Failed to create " NSH_TEMPDIR);
        return -1;
    }

    return 0;
}

int nsh_engine_startup()
{
    int err;

    err = nsh_engine_workdir_create() ||
          nsh_engine_tempdir_create() ||
          nsh_database_create()       ||
          cntr_settings_set_root(NSH_TEMPDIR);

    return err ? -1 : 0;
}

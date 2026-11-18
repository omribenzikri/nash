#include <stdio.h>
#include "engine/persistence/database.h"
#include "engine/startup.h"

int nsh_engine_startup()
{
    int err;

    err = nsh_database_create();
    if (err) {
        fprintf(stderr, "[x] Failed to initialize the engine database\n");
        return -1;
    }

    return 0;
}

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <sqlite3.h>
#include "common/constants.h"
#include "engine/persistence/repos/images.h"
#include "engine/persistence/repos/blobs.h"
#include "engine/persistence/internal/uuid.h"
#include "engine/persistence/internal/database.h" // IWYU pragma: keep
#include "engine/persistence/database.h"

#define NSH_DB_PATH NSH_WORKDIR "/nash.db"

static int nsh_database_repos_init(nsh_database_t *db)
{
    int err;

    // For my own sanity (TODO: evaluate at compile time?)
    assert(NSH_UUID_TYPE_LEN == sizeof(uuid_t));

    err = nsh_images_repo_create(db);
    if (err)
        return -1;

    err = nsh_blobs_repo_create(db);
    if (err)
        return -1;

    return 0;
}

int nsh_database_create()
{
    nsh_database_t db;
    int retcode;
    int err;
    int ret = 0;

    retcode = sqlite3_open_v2(NSH_DB_PATH, &db.conn,
        SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, NULL);

    if (retcode != SQLITE_OK) {
        fprintf(stderr, "[x] Failed to open database: %s\n", sqlite3_errmsg(db.conn));
        ret = -1;
        goto out;
    }

    err = nsh_database_repos_init(&db);
    if (err) {
        ret = -1;
        goto out_close_conn;
    }

out_close_conn:
    sqlite3_close(db.conn);
out:
    return ret;
}

nsh_database_t *nsh_database_open()
{
    nsh_database_t *db;
    int retcode;

    db = malloc(sizeof(*db));
    if (db == NULL) {
        perror("[x] Failed to allocate database");
        goto error;
    }

    retcode = sqlite3_open_v2(NSH_DB_PATH, &db->conn,
        SQLITE_OPEN_READWRITE, NULL);

    if (retcode != SQLITE_OK) {
        fprintf(stderr, "[x] Failed to open database: %s\n", sqlite3_errmsg(db->conn));
        goto error_free_db;
    }

    return db;

error_free_db:
    free(db);
error:
    return NULL;
}

void nsh_database_close(nsh_database_t *db)
{
    sqlite3_close(db->conn);
    free(db);
}

#include <stdio.h>
#include <sqlite3.h>
#include <uuid/uuid.h>
#include "engine/persistence/database.h"
#include "engine/persistence/internal/database.h"
#include "engine/persistence/repos/images.h"

static const char *nsh_images_create_table_sql =
    "CREATE TABLE IF NOT EXISTS images ("
    "   id " NSH_UUID_TYPE " PRIMARY KEY,"
    "   name TEXT NOT NULL UNIQUE,"
    "   ref TEXT NOT NULL,"
    "   created_at INTEGER NOT NULL"
    ");";

static const char *nsh_images_select_by_name_sql =
    "SELECT id, name, ref, created_at "
    "FROM images "
    "WHERE name = ?;";

static const char *nsh_images_insert_sql =
    "INSERT INTO images (id, name, ref, created_at) "
    "VALUES (?, ?, ?, ?);";

static void nsh_images_row_load(sqlite3_stmt *stmt, nsh_image_entity_t *entity)
{
    uuid_copy(entity->id, nsh_column_uuid(stmt, 0));
    entity->name = (const char *)sqlite3_column_text(stmt, 1);
    entity->ref = (const char *)sqlite3_column_text(stmt, 2);
    entity->created_at = sqlite3_column_int64(stmt, 3);
}

int nsh_images_repo_create(nsh_database_t *db)
{
    char *error_message = NULL;
    int retcode;

    retcode = sqlite3_exec(db->conn,
        nsh_images_create_table_sql,
        NULL, NULL, &error_message);

    if (retcode != SQLITE_OK) {
        fprintf(stderr, "[x] Failed to create images table: %s\n", error_message);
        sqlite3_free(error_message);
        return -1;
    }

    return 0;
}

int nsh_images_repo_iter_by_name(
    nsh_database_t *db, const char *name,
    int (*cb)(nsh_image_entity_t *, void *), void *data)
{
    nsh_image_entity_t entity;
    sqlite3_stmt *stmt;
    int err;
    int retcode;
    int ret = 0;

    retcode = sqlite3_prepare_v2(db->conn,
        nsh_images_select_by_name_sql, -1, &stmt, NULL);

    if (retcode != SQLITE_OK) {
        fprintf(stderr,
            "[x] Failed to prepare query statement: %s\n",
            sqlite3_errmsg(db->conn));
        ret = -1;
        goto out;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    while ((retcode = sqlite3_step(stmt)) == SQLITE_ROW) {
        nsh_images_row_load(stmt, &entity);

        err = cb(&entity, data);
        if (err) {
            ret = -1;
            goto out_finalize_stmt;
        }
    }

    if (retcode != SQLITE_DONE) {
        fprintf(stderr, "[x] Failed to query images: %s\n", sqlite3_errmsg(db->conn));
        ret = -1;
        goto out_finalize_stmt;
    }

out_finalize_stmt:
    sqlite3_finalize(stmt);
out:
    return ret;
}

int nsh_images_repo_add(nsh_database_t *db, nsh_image_entity_t *entity)
{
    sqlite3_stmt *stmt;
    int retcode;
    int ret = 0;

    retcode = sqlite3_prepare_v2(db->conn,
        nsh_images_insert_sql, -1, &stmt, NULL);

    if (retcode != SQLITE_OK) {
        fprintf(stderr,
            "[x] Failed to prepare insertion statement: %s\n",
            sqlite3_errmsg(db->conn));
        ret = -1;
        goto out;
    }

    sqlite3_bind_blob(stmt, 1, entity->id, sizeof(entity->id), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, entity->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, entity->ref, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, entity->created_at);

    retcode = sqlite3_step(stmt);
    if (retcode != SQLITE_DONE) {
        fprintf(stderr, "[x] Failed to insert image: %s\n", sqlite3_errmsg(db->conn));
        ret = -1;
        goto out_finalize_stmt;
    }

out_finalize_stmt:
    sqlite3_finalize(stmt);
out:
    return ret;
}

static int nsh_images_repo_exists_cb(nsh_image_entity_t *entity, void *exists)
{
    (void)entity;
    *(bool *)exists = true;

    return 0;
}

int nsh_images_repo_exists_by_name(nsh_database_t *db, const char *name, bool *out)
{
    bool exists = false;
    int err;

    err = nsh_images_repo_iter_by_name(db, name, nsh_images_repo_exists_cb, &exists);
    if (err)
        return -1;

    *out = exists;
    return 0;
}

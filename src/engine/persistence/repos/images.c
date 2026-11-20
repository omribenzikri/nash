#include <stdio.h>
#include <sqlite3.h>
#include <uuid/uuid.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "common/entities/image.h"
#include "engine/persistence/database.h"
#include "engine/persistence/internal/database.h" // IWYU pragma: keep
#include "engine/persistence/internal/uuid.h"
#include "engine/persistence/repos/images.h"

static const char *nsh_images_create_table_sql =
    "CREATE TABLE IF NOT EXISTS images ("
    "   id " NSH_UUID_TYPE " PRIMARY KEY,"
    "   name TEXT NOT NULL UNIQUE,"
    "   ref TEXT NOT NULL,"
    "   created_at INTEGER NOT NULL"
    ");";

static const char *nsh_images_select_sql =
    "SELECT id, name, ref, created_at "
    "FROM images;";

static const char *nsh_images_select_by_name_sql =
    "SELECT id, name, ref, created_at "
    "FROM images "
    "WHERE name = ?;";

static const char *nsh_images_insert_sql =
    "INSERT INTO images (id, name, ref, created_at) "
    "VALUES (?, ?, ?, ?);";

static const char *nsh_images_delete_by_id_sql =
    "DELETE FROM images "
    "WHERE id = ?;";

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

static nsh_image_entity_t *nsh_images_repo_load_row(sqlite3_stmt *stmt)
{
    const unsigned char *id = nsh_column_uuid(stmt, 0);
    const char *name = (const char *)sqlite3_column_text(stmt, 1);
    const char *ref = (const char *)sqlite3_column_text(stmt, 2);
    const time_t created_at = sqlite3_column_int64(stmt, 3);

    return nsh_image_entity_new(id, name, ref, created_at);
}

static int nsh_images_repo_append_row(sqlite3_stmt *stmt, struct cc_array_s *records)
{
    nsh_image_entity_t *entity;
    enum cc_stat cc_stat;

    entity = nsh_images_repo_load_row(stmt);
    if (entity == NULL)
        goto error;

    cc_stat = cc_array_add(records, entity);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to append image entity to query results\n");
        goto error_free_clone;
    }

    return 0;

error_free_clone:
    nsh_image_entity_free(entity);
error:
    return -1;
}

static int nsh_images_repo_all_internal(nsh_database_t *db, struct cc_array_s *records)
{
    sqlite3_stmt *stmt;
    int err;
    int retcode;
    int ret = 0;

    retcode = sqlite3_prepare_v2(db->conn, nsh_images_select_sql, -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        fprintf(stderr,
            "[x] Failed to prepare images query statement: %s\n",
            sqlite3_errmsg(db->conn));
        ret = -1;
        goto out;
    }

    while ((retcode = sqlite3_step(stmt)) == SQLITE_ROW) {
        err = nsh_images_repo_append_row(stmt, records);
        if (err) {
            ret = -1;
            goto out_finalize_stmt;
        }
    }

    if (retcode != SQLITE_DONE) {
        fprintf(stderr,
            "[x] Failed to perform images query: %s\n",
            sqlite3_errmsg(db->conn));
        ret = -1;
        goto out_finalize_stmt;
    }

out_finalize_stmt:
    sqlite3_finalize(stmt);
out:
    return ret;
}

int nsh_images_repo_all(nsh_database_t *db, struct cc_array_s **out)
{
    struct cc_array_s *records;
    enum cc_stat cc_stat;
    int err;

    cc_stat = cc_array_new(&records);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate images query results array\n");
        goto error;
    }

    err = nsh_images_repo_all_internal(db, records);
    if (err)
        goto error_destroy_records;

    *out = records;
    return 0;

error_destroy_records:
    cc_array_destroy_cb(records, (void (*)(void *))nsh_image_entity_free);
error:
    return -1;
}

int nsh_images_repo_first_by_name(nsh_database_t *db, const char *name, nsh_image_entity_t **out)
{
    nsh_image_entity_t *entity = NULL;
    sqlite3_stmt *stmt;
    int retcode;
    int ret = 0;

    retcode = sqlite3_prepare_v2(db->conn,
        nsh_images_select_by_name_sql, -1, &stmt, NULL);

    if (retcode != SQLITE_OK) {
        fprintf(stderr,
            "[x] Failed to prepare images query statement: %s\n",
            sqlite3_errmsg(db->conn));
        ret = -1;
        goto out;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    while ((retcode = sqlite3_step(stmt)) == SQLITE_ROW) {
        entity = nsh_images_repo_load_row(stmt);
        if (entity == NULL) {
            ret = -1;
            goto out_finalize_stmt;
        }

        retcode = SQLITE_DONE;
        break;
    }

    if (retcode != SQLITE_DONE) {
        fprintf(stderr,
            "[x] Failed to perform images query: %s\n",
            sqlite3_errmsg(db->conn));
        ret = -1;
        goto out_finalize_stmt;
    }

    *out = entity;

out_finalize_stmt:
    sqlite3_finalize(stmt);
out:
    return ret;
}

int nsh_images_repo_exists_by_name(nsh_database_t *db, const char *name, bool *out)
{
    nsh_image_entity_t *entity;
    int err;

    err = nsh_images_repo_first_by_name(db, name, &entity);
    if (err)
        return -1;

    if (entity != NULL) {
        nsh_image_entity_free(entity);
        *out = true;
    }
    else
        *out = false;

    return 0;
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

    nsh_bind_uuid(stmt, 1, entity->id, sizeof(entity->id), SQLITE_STATIC);
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

int nsh_images_repo_delete_by_id(nsh_database_t *db, uuid_t id)
{
    sqlite3_stmt *stmt;
    int retcode;
    int ret = 0;

    retcode = sqlite3_prepare_v2(db->conn,
        nsh_images_delete_by_id_sql, -1, &stmt, NULL);

    if (retcode != SQLITE_OK) {
        fprintf(stderr,
            "[x] Failed to prepare deletion statement: %s\n",
            sqlite3_errmsg(db->conn));
        ret = -1;
        goto out;
    }

    nsh_bind_uuid(stmt, 1, id, sizeof(uuid_t), SQLITE_STATIC);

    retcode = sqlite3_step(stmt);
    if (retcode != SQLITE_DONE) {
        fprintf(stderr, "[x] Failed to delete image: %s\n", sqlite3_errmsg(db->conn));
        ret = -1;
        goto out_finalize_stmt;
    }

out_finalize_stmt:
    sqlite3_finalize(stmt);
out:
    return ret;
}

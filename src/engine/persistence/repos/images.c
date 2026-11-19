#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <uuid/uuid.h>
#include "engine/persistence/database.h"
#include "engine/persistence/internal/database.h"
#include "engine/persistence/internal/uuid.h"
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

static const char *nsh_images_delete_by_id_sql =
    "DELETE FROM images "
    "WHERE id = ?;";

nsh_image_entity_t *nsh_image_entity_alloc(
    const uuid_t id, const char *name, const char *ref, time_t created_at)
{
    nsh_image_entity_t *entity;

    entity = malloc(sizeof(*entity));
    if (entity == NULL) {
        perror("[x] Failed to allocate image entity");
        goto error;
    }

    uuid_copy(entity->id, id);

    entity->name = strdup(name);
    if (entity->name == NULL) {
        perror("[x] Failed to copy image entity name");
        goto error_free_entity;
    }

    entity->ref = strdup(ref);
    if (entity->ref == NULL) {
        perror("[x] Failed to copy image ref");
        goto error_free_name;
    }

    entity->created_at = created_at;

    return entity;

error_free_name:
    free(entity->name);
error_free_entity:
    free(entity);
error:
    return NULL;
}

nsh_image_entity_t *nsh_image_entity_clone(nsh_image_entity_t *entity)
{
    return nsh_image_entity_alloc(
        entity->id, entity->name, entity->ref, entity->created_at);
}

void nsh_image_entity_free(nsh_image_entity_t *entity)
{
    free(entity->name);
    free(entity->ref);
    free(entity);
}

static nsh_image_entity_t *nsh_image_entity_load(sqlite3_stmt *stmt)
{
    return nsh_image_entity_alloc(
        nsh_column_uuid(stmt, 0),
        (const char *)sqlite3_column_text(stmt, 1),
        (const char *)sqlite3_column_text(stmt, 2),
        sqlite3_column_int64(stmt, 3)
    );
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

static int nsh_images_repo_iter_process(sqlite3_stmt *stmt,
    int (*cb)(nsh_image_entity_t *, void *), void *data)
{
    nsh_image_entity_t *entity;
    int ret;

    entity = nsh_image_entity_load(stmt);
    if (entity == NULL)
        return NSH_DB_ITER_ERR;

    ret = cb(entity, data);
    nsh_image_entity_free(entity);

    return ret;
}

int nsh_images_repo_iter_by_name(
    nsh_database_t *db, const char *name,
    int (*cb)(nsh_image_entity_t *, void *), void *data)
{
    sqlite3_stmt *stmt;
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
        retcode = nsh_images_repo_iter_process(stmt, cb, data);
        if (retcode == NSH_DB_ITER_ERR) {
            ret = -1;
            goto out_finalize_stmt;
        }
        if (retcode == NSH_DB_ITER_STOP) {
            retcode = SQLITE_DONE;
            break;
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

static int nsh_images_repo_exists_cb(nsh_image_entity_t *entity, void *exists)
{
    (void)entity;
    *(bool *)exists = true;

    return NSH_DB_ITER_STOP;
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

static int nsh_images_repo_first_cb(nsh_image_entity_t *entity, void *out)
{
    nsh_image_entity_t *clone;

    clone = nsh_image_entity_clone(entity);
    if (clone == NULL)
        return NSH_DB_ITER_ERR;

    *(nsh_image_entity_t **)out = clone;

    return NSH_DB_ITER_STOP;
}

int nsh_images_repo_first_by_name(nsh_database_t *db, const char *name, nsh_image_entity_t **out)
{
    nsh_image_entity_t *entity = NULL;
    int err;

    err = nsh_images_repo_iter_by_name(
        db, name, nsh_images_repo_first_cb, &entity);
    if (err)
        return -1;

    *out = entity;
    return 0;
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

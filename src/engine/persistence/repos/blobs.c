#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>
#include "engine/common/entities/blob.h"
#include "engine/persistence/internal/database.h" // IWYU pragma: keep
#include "engine/persistence/repos/blobs.h"

static const char *nsh_blobs_create_table_sql =
    "CREATE TABLE IF NOT EXISTS blobs ("
    "   digest TEXT PRIMARY KEY CHECK(length(digest) > 0),"
    "   size INTEGER CHECK(size >= 0),"
    "   refcount INTEGER NOT NULL CHECK(refcount >= 0)"
    ");";

static const char *nsh_blobs_acquire_sql =
    "INSERT INTO blobs (digest, size, refcount) "
    "VALUES (?, ?, 1) "
    "ON CONFLICT DO UPDATE "
    "SET refcount = refcount + 1 "
    "RETURNING refcount;";

static const char *nsh_blobs_release_decrement_sql =
    "UPDATE blobs "
    "SET refcount = refcount - 1 "
    "WHERE digest = ? "
    "RETURNING refcount;";

static const char *nsh_blobs_release_delete_sql =
    "DELETE FROM blobs "
    "WHERE digest = ?;";

static const char *nsh_blobs_select_by_digest_sql =
    "SELECT digest, size, refcount "
    "FROM blobs "
    "WHERE digest = ?;";

static nsh_blob_entity_t *nsh_blobs_repo_load_row(sqlite3_stmt *stmt)
{
    const char *digest = (const char *)sqlite3_column_text(stmt, 0);
    const int64_t size = sqlite3_column_int64(stmt, 1);
    const int refcount = sqlite3_column_int64(stmt, 2);

    return nsh_blob_entity_new(digest, size, refcount);
}

int nsh_blobs_repo_create(nsh_database_t *db)
{
    char *error_message = NULL;
    int retcode;

    retcode = sqlite3_exec(db->conn,
        nsh_blobs_create_table_sql,
        NULL, NULL, &error_message);

    if (retcode != SQLITE_OK) {
        fprintf(stderr, "[x] Failed to create blobs table: %s\n", error_message);
        sqlite3_free(error_message);
        return -1;
    }

    return 0;
}

int nsh_blobs_repo_acquire(nsh_database_t *db, const char *digest, int64_t size)
{
    sqlite3_stmt *stmt;
    int retcode;
    int ret = -1;

    retcode = sqlite3_prepare_v2(db->conn,
        nsh_blobs_acquire_sql, -1, &stmt, NULL);

    if (retcode != SQLITE_OK) {
        fprintf(stderr,
            "[x] Failed to prepare upsert statement: %s\n",
            sqlite3_errmsg(db->conn));
        goto out;
    }

    sqlite3_bind_text(stmt, 1, digest, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, size);

    retcode = sqlite3_step(stmt);
    if (retcode != SQLITE_ROW) {
        fprintf(stderr,
            "[x] Failed to upsert blob: %s\n", sqlite3_errmsg(db->conn));
        goto out_finalize_stmt;
    }

    ret = sqlite3_column_int(stmt, 0);

out_finalize_stmt:
    sqlite3_finalize(stmt);
out:
    return ret;
}

static int nsh_blobs_repo_release_decrement(
    nsh_database_t *db, const char *digest, int *out)
{
    sqlite3_stmt *stmt;
    int retcode;
    int ret = 0;

    retcode = sqlite3_prepare_v2(db->conn,
        nsh_blobs_release_decrement_sql,
        -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        fprintf(stderr,
            "[x] Failed to prepare update statement: %s\n",
            sqlite3_errmsg(db->conn));
        ret = -1;
        goto out;
    }

    sqlite3_bind_text(stmt, 1, digest, -1, SQLITE_STATIC);

    retcode = sqlite3_step(stmt);
    if (retcode != SQLITE_ROW) {
        fprintf(stderr, "[x] Failed to update blob: %s\n", sqlite3_errmsg(db->conn));
        ret = -1;
        goto out_finalize_stmt;
    }

    *out = sqlite3_column_int(stmt, 0);

out_finalize_stmt:
    sqlite3_finalize(stmt);
out:
    return ret;
}

static int nsh_blobs_repo_release_delete(nsh_database_t *db, const char *digest)
{
    sqlite3_stmt *stmt;
    int retcode;
    int ret = 0;

    retcode = sqlite3_prepare_v2(db->conn,
        nsh_blobs_release_delete_sql,
        -1, &stmt, NULL);
    if (retcode != SQLITE_OK) {
        fprintf(stderr,
            "[x] Failed to prepare delete statement: %s\n",
            sqlite3_errmsg(db->conn));
        ret = -1;
        goto out;
    }

    sqlite3_bind_text(stmt, 1, digest, -1, SQLITE_STATIC);

    retcode = sqlite3_step(stmt);
    if (retcode != SQLITE_DONE) {
        fprintf(stderr, "[x] Failed to delete blob: %s\n", sqlite3_errmsg(db->conn));
        ret = -1;
        goto out_finalize_stmt;
    }

out_finalize_stmt:
    sqlite3_finalize(stmt);
out:
    return ret;
}

int nsh_blobs_repo_release(nsh_database_t *db, const char *digest)
{
    int refcount;
    int retcode;
    int err;

    retcode = sqlite3_exec(db->conn, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    if (retcode != SQLITE_OK) {
        fprintf(stderr,
            "[x] Failed to begin transaction: %s\n", sqlite3_errmsg(db->conn));
        goto error;
    }

    err = nsh_blobs_repo_release_decrement(db, digest, &refcount);
    if (err)
        goto error_rollback;

    if (refcount == 0) {
        err = nsh_blobs_repo_release_delete(db, digest);
        if (err)
            goto error_rollback;
    }

    retcode = sqlite3_exec(db->conn, "COMMIT", NULL, NULL, NULL);
    if (retcode != SQLITE_OK) {
        fprintf(stderr,
            "[x] Failed to commit transaction: %s\n", sqlite3_errmsg(db->conn));
        goto error_rollback;
    }

    return refcount;

error_rollback:
    retcode = sqlite3_exec(db->conn, "ROLLBACK", NULL, NULL, NULL);
    if (retcode != SQLITE_OK)
        fprintf(stderr,
            "[x] Failed to rollback transaction: %s\n", sqlite3_errmsg(db->conn));
error:
    return -1;
}

int nsh_blobs_repo_first_by_digest(
    nsh_database_t *db, const char *digest, nsh_blob_entity_t **out)
{
    nsh_blob_entity_t *entity = NULL;
    sqlite3_stmt *stmt;
    int retcode;
    int ret = 0;

    retcode = sqlite3_prepare_v2(db->conn,
        nsh_blobs_select_by_digest_sql, -1, &stmt, NULL);

    if (retcode != SQLITE_OK) {
        fprintf(stderr,
            "[x] Failed to prepare blobs query statement: %s\n",
            sqlite3_errmsg(db->conn));
        ret = -1;
        goto out;
    }

    sqlite3_bind_text(stmt, 1, digest, -1, SQLITE_STATIC);

    while ((retcode = sqlite3_step(stmt)) == SQLITE_ROW) {
        entity = nsh_blobs_repo_load_row(stmt);
        if (entity == NULL) {
            ret = -1;
            goto out_finalize_stmt;
        }

        retcode = SQLITE_DONE;
        break;
    }

    if (retcode != SQLITE_DONE) {
        fprintf(stderr,
            "[x] Failed to perform blobs query: %s\n",
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

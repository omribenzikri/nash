#include <stdio.h>
#include <time.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include "engine/persistence/database.h"
#include "engine/persistence/repos/blobs.h"
#include "engine/persistence/repos/images.h"
#include "engine/image/oci/lifecycle.h"
#include "engine/image/lifecycle.h"

static int nsh_image_import_content(
    nsh_database_t *db, const char *path, char *outb, size_t outl)
{
    nsh_oci_lc_context_t ctx;
    int err;
    int ret = 0;

    err = nsh_oci_image_import(path, &ctx);
    if (err) {
        ret = -1;
        goto out;
    }

    snprintf(outb, outl, "%s", ctx.ref);

    for (size_t i = 0; i < ctx.blob_digests_len; i++) {
        err = nsh_blobs_repo_acquire(db, ctx.blob_digests[i]);
        if (err) {
            ret = -1;
            goto out_destroy_lc_context;
        }
    }

out_destroy_lc_context:
    nsh_oci_lc_context_destroy(&ctx);
out:
    return ret;
}

static int nsh_image_import_persist(nsh_database_t *db, const char *name, const char *ref)
{
    nsh_image_entity_t entity;
    int err;

    uuid_generate(entity.id);
    entity.name = (char *)name;
    entity.ref = (char *)ref;
    entity.created_at = time(NULL);

    err = nsh_images_repo_add(db, &entity);
    if (err)
        return -1;

    return 0;
}

static int nsh_image_import_internal(
    const char *path, const char *name, nsh_database_t *db)
{
    int err;
    bool image_exists;
    char image_ref[NAME_MAX + 1];

    err = nsh_images_repo_exists_by_name(db, name, &image_exists);
    if (err)
        return -1;
    if (image_exists) {
        fprintf(stderr, "[x] Failed to import image: %s: image already exists\n", name);
        return -1;
    }

    err = nsh_image_import_content(db, path, image_ref, sizeof(image_ref));
    if (err)
        return -1;

    err = nsh_image_import_persist(db, name, image_ref);
    if (err)
        return -1;

    return 0;
}

int nsh_image_import(const char *path, const char *name)
{
    nsh_database_t *db;
    int err;
    int ret = 0;

    db = nsh_database_open();
    if (db == NULL) {
        ret = -1;
        goto out;
    }

    err = nsh_image_import_internal(path, name, db);
    if (err) {
        ret = -1;
        goto out_close_db;
    }

out_close_db:
    nsh_database_close(db);
out:
    return ret;
}

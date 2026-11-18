#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include "engine/persistence/database.h"
#include "engine/persistence/repos/images.h"
#include "engine/image/oci/lifecycle.h"
#include "engine/image/lifecycle.h"

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
    char *ref;
    bool exists;
    int err;
    int ret = 0;

    err = nsh_images_repo_exists_by_name(db, name, &exists);
    if (err) {
        ret = -1;
        goto out;
    }
    if (exists) {
        fprintf(stderr, "[x] Failed to import image: %s: image already exists\n", name);
        ret = -1;
        goto out;
    }

    err = nsh_oci_image_import(path, &ref);
    if (err) {
        ret = -1;
        goto out;
    }

    err = nsh_image_import_persist(db, name, ref);
    if (err) {
        ret = -1;
        goto out_free_ref;
    }

out_free_ref:
    free(ref);
out:
    return ret;
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

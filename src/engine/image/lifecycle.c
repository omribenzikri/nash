#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "common/constants.h"
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

    err = nsh_oci_image_import(NSH_WORKDIR_IMAGES, path, &ref);
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

int nsh_image_list(struct cc_array_s **out)
{
    nsh_database_t *db;
    int err;
    int ret = 0;

    db = nsh_database_open();
    if (db == NULL) {
        ret = -1;
        goto out;
    }

    err = nsh_images_repo_all(db, out);
    if (err) {
        ret = -1;
        goto out_close_db;
    }

out_close_db:
    nsh_database_close(db);
out:
    return ret;
}


int nsh_image_delete(const char *name)
{
    nsh_database_t *db;
    nsh_image_entity_t *entity;
    int err;
    int ret = 0;

    db = nsh_database_open();
    if (db == NULL) {
        ret = -1;
        goto out;
    }

    err = nsh_images_repo_first_by_name(db, name, &entity);
    if (err) {
        ret = -1;
        goto out_close_db;
    }
    if (entity == NULL) {
        fprintf(stderr, "[x] Failed to remove image: %s: image does not exist\n", name);
        ret = -1;
        goto out_close_db;
    }

    err = nsh_oci_image_delete(NSH_WORKDIR_IMAGES, entity->ref);
    if (err) {
        ret = -1;
        goto out_free_entity;
    }

    err = nsh_images_repo_delete_by_id(db, entity->id);
    if (err) {
        ret = -1;
        goto out_free_entity;
    }

out_free_entity:
    nsh_image_entity_free(entity);
out_close_db:
    nsh_database_close(db);
out:
    return ret;
}

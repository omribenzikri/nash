#include <stdio.h>
#include <time.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include "common/constants.h"
#include "common/utils/filesystem.h"
#include "common/utils/macros.h"
#include "engine/persistence/database.h"
#include "engine/persistence/repos/blobs.h"
#include "engine/persistence/repos/images.h"
#include "engine/image/oci.h"
#include "engine/image/lifecycle.h"

// Assuming the top-level workdir exists
static int nsh_image_store_create_if_missing()
{
    int err;

    err = mkdir_if_missing(NSH_STORE_SNAPSHOTS, S_IRWXU);
    if (err) {
        perror("[x] Failed to create " NSH_STORE_SNAPSHOTS);
        return -1;
    }

    err = mkdir_if_missing(NSH_STORE_MANIFESTS, S_IRWXU);
    if (err) {
        perror("[x] Failed to create " NSH_STORE_MANIFESTS);
        return -1;
    }

    err = mkdir_if_missing(NSH_STORE_CONFIG, S_IRWXU);
    if (err) {
        perror("[x] Failed to create " NSH_STORE_CONFIG);
        return -1;
    }

    return 0;
}

static int nsh_image_import_manifest(
    const char *path, nsh_oci_image_meta_t *image_meta, nsh_database_t *db)
{
    char *digest;
    int err;

    err = nsh_oci_import_manifest(NSH_STORE_MANIFESTS, path, image_meta, &digest);
    if (err)
        return -1;

    err = nsh_blobs_repo_acquire(db, digest);
    if (err)
        return -1;

    return 0;
}

static int nsh_image_import_config(
    const char *path, nsh_oci_image_meta_t *image_meta, nsh_database_t *db)
{
    char *digest;
    int err;

    err = nsh_oci_import_config(NSH_STORE_CONFIG, path, image_meta, &digest);
    if (err)
        return -1;

    err = nsh_blobs_repo_acquire(db, digest);
    if (err)
        return -1;

    return 0;
}

static int nsh_image_import_layers(
    const char *path, nsh_oci_image_meta_t *image_meta, nsh_database_t *db)
{
    ARRAY_WITH_LEN(char *, digests);
    int err;
    int ret = 0;

    err = nsh_oci_import_layers(
        NSH_STORE_SNAPSHOTS, path, image_meta,
        &digests, &digests_len);
    if (err) {
        ret = -1;
        goto out;
    }

    for (size_t i = 0; i < digests_len; i++) {
        err = nsh_blobs_repo_acquire(db, digests[i]);
        if (err) {
            ret = -1;
            goto out_free_digests;
        }
    }

out_free_digests:
    free(digests);
out:
    return ret;
}

static int nsh_image_import_blobs(
    const char *path, nsh_database_t *db, char *outb, size_t outl)
{
    nsh_oci_image_meta_t image_meta;
    int err;
    int ret = 0;

    err = nsh_oci_image_meta_load(&image_meta, path);
    if (err) {
        ret = -1;
        goto out;
    }

    err = nsh_image_import_manifest(path, &image_meta, db);
    if (err) {
        ret = -1;
        goto out_destroy_image_meta;
    }

    err = nsh_image_import_config(path, &image_meta, db);
    if (err) {
        ret = -1;
        goto out_destroy_image_meta;
    }

    err = nsh_image_import_layers(path, &image_meta, db);
    if (err) {
        ret = -1;
        goto out_destroy_image_meta;
    }

    snprintf(outb, outl, "%s", image_meta.ref);

out_destroy_image_meta:
    nsh_oci_image_meta_destroy(&image_meta);
out:
    return ret;
}

static int nsh_image_import_persist(nsh_database_t *db, const char *name, const char *ref)
{
    nsh_image_entity_t entity;
    int err;

    uuid_generate(entity.id);
    entity.name = name;
    entity.ref = ref;
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

    err = nsh_image_store_create_if_missing();
    if (err)
        return -1;

    err = nsh_image_import_blobs(path, db, image_ref, sizeof(image_ref));
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

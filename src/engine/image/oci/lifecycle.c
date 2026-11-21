#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <linux/limits.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "common/utils/hashing.h"
#include "common/utils/string.h"
#include "common/utils/filesystem.h"
#include "common/entities/blob.h"
#include "engine/persistence/repos/blobs.h"
#include "engine/persistence/database.h"
#include "engine/image/oci/common.h"
#include "engine/image/oci/parse.h"
#include "engine/image/oci/bundle.h"
#include "engine/image/oci/store.h"
#include "engine/image/oci/unpack.h"
#include "engine/image/oci/lifecycle.h"

typedef enum {
    NSH_OCI_BLOB_MANIFEST,
    NSH_OCI_BLOB_CONFIG,
    NSH_OCI_BLOB_LAYER
} nsh_oci_blob_type_t;

typedef struct {
    const char *digest;
    const char *media_type;
    int64_t size;
    nsh_oci_blob_type_t type;
    char source_path[PATH_MAX];
    char target_path[PATH_MAX];
} nsh_oci_import_blob_t;

typedef struct {
    const char *store_path;
    const char *bundle_path;
    nsh_database_t *db;
    nsh_oci_image_desc_t *desc;
    nsh_oci_image_meta_t meta;
    struct cc_array_s *imported;
} nsh_oci_import_context_t;

typedef struct {
    const char *store_path;
    const char *bundle_path;
    const char *ref;
    nsh_oci_image_meta_t meta;
} nsh_oci_export_context_t;

static int nsh_oci_import_context_init(nsh_oci_import_context_t *ctx,
    const char *store_path, const char *bundle_path, nsh_oci_image_desc_t *desc)
{
    enum cc_stat cc_stat;
    int err;

    memset(ctx, 0, sizeof(*ctx));
    ctx->store_path = store_path;
    ctx->bundle_path = bundle_path;
    ctx->desc = desc;

    cc_stat = cc_array_new(&ctx->imported);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate imported blobs array\n");
        goto error;
    }

    ctx->db = nsh_database_open();
    if (ctx->db == NULL)
        goto error_destroy_imported;

    err = nsh_oci_bundle_load_image_meta(&ctx->meta, bundle_path,
        ctx->desc->ref, sizeof(ctx->desc->ref));
    if (err)
        goto error_close_db;

    return 0;

error_close_db:
    nsh_database_close(ctx->db);
error_destroy_imported:
    cc_array_destroy(ctx->imported);
error:
    return -1;
}

static void nsh_oci_import_context_destroy(nsh_oci_import_context_t *ctx)
{
    nsh_oci_image_meta_destroy(&ctx->meta);
    nsh_database_close(ctx->db);
    cc_array_destroy_cb(ctx->imported, free);
}

static int nsh_oci_export_context_init(nsh_oci_export_context_t *ctx,
    const char *store_path, const char *ref, const char *bundle_path)
{
    int err;

    memset(ctx, 0, sizeof(*ctx));
    ctx->store_path = store_path;
    ctx->bundle_path = bundle_path;
    ctx->ref = ref;

    err = nsh_oci_store_load_image_meta(&ctx->meta, store_path, ref);
    if (err)
        return -1;

    return 0;
}

static void nsh_oci_export_context_destroy(nsh_oci_export_context_t *ctx)
{
    nsh_oci_image_meta_destroy(&ctx->meta);
}

static int64_t nsh_oci_image_size_calc(struct cc_array_s *blobs)
{
    nsh_oci_import_blob_t *blob;
    int64_t total = 0;

    for (size_t i = 0; i < cc_array_size(blobs); i++) {
        cc_array_get_at(blobs, i, (void **)&blob);
        total += blob->size;
    }

    return total;
}

static nsh_oci_blob_type_t nsh_oci_blob_classify(const char *media_type)
{
    if (strcmp(media_type, OCI_MEDIA_TYPE_MANIFEST) == 0)
        return NSH_OCI_BLOB_MANIFEST;
    if (strcmp(media_type, OCI_MEDIA_TYPE_CONFIG) == 0)
        return NSH_OCI_BLOB_CONFIG;
    return NSH_OCI_BLOB_LAYER;
}

static void nsh_oci_blob_path_resolve(
    const char *store_path, const char *digest,
    nsh_oci_blob_type_t type, char *buf, size_t size)
{
    switch (type) {
        case NSH_OCI_BLOB_MANIFEST:
            return nsh_oci_store_manifest_path_resolve(
                store_path, digest, buf, size);
        case NSH_OCI_BLOB_CONFIG:
            return nsh_oci_store_config_path_resolve(
                store_path, digest, buf, size);
        case NSH_OCI_BLOB_LAYER:
            return nsh_oci_store_snapshot_path_resolve(
                store_path, digest, buf, size);
        default:
            assert(false && "Unhandled blob type");
    }
}

static int64_t nsh_oci_blob_size_query(
    nsh_database_t *db, const char *digest)
{
    nsh_blob_entity_t *entity;
    int64_t size;
    int err;

    err = nsh_blobs_repo_first_by_digest(db, digest, &entity);
    if (err)
        return -1;

    size = entity->size;
    nsh_blob_entity_free(entity);

    return size;
}

static int nsh_oci_import_blob_init(
    nsh_oci_import_blob_t *blob, nsh_oci_import_context_t *ctx,
    const char *digest, const char *media_type)
{
    int err;

    blob->digest = digest;
    blob->media_type = media_type;
    blob->type = nsh_oci_blob_classify(media_type);

    err = nsh_oci_bundle_blob_path_resolve(ctx->bundle_path, digest,
        blob->source_path, sizeof(blob->source_path));
    if (err)
        return -1;

    nsh_oci_blob_path_resolve(ctx->store_path, blob->digest,
        blob->type, blob->target_path, sizeof(blob->target_path));

    return 0;
}

static int nsh_oci_blob_commit(
    struct cc_array_s *imported, nsh_oci_import_blob_t *blob)
{
    nsh_oci_import_blob_t *clone;
    enum cc_stat cc_stat;

    clone = malloc(sizeof(*clone));
    if (clone == NULL) {
        perror("[x] Failed to allocate imported blob");
        goto error;
    }
    memcpy(clone, blob, sizeof(*blob));

    cc_stat = cc_array_add(imported, clone);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to add imported blob: %s\n", clone->digest);
        goto error_free_clone;
    }

    return 0;

error_free_clone:
    free(clone);
error:
    return -1;
}

static int nsh_oci_verify_blob_hash(const char *path,
    const char *alg, const char *encoded, bool *out)
{
    union {
        uint8_t sha256[crypto_hash_sha256_BYTES];
        uint8_t sha512[crypto_hash_sha512_BYTES];
        uint8_t blake3[BLAKE3_OUT_LEN];
    } expected;
    ssize_t expected_len;

    expected_len = str_parse_hex(encoded, (uint8_t *)&expected, sizeof(expected));
    if (expected_len == -1) {
        fprintf(stderr, "[x] Failed to parse digest encoded: %s\n", encoded);
        return -1;
    }

    if (strcmp(alg, OCI_HASH_ALG_SHA256) == 0) {
        if (expected_len != sizeof(expected.sha256)) {
            *out = false;
            return -1;
        }
        return sha256_verify_file(path, expected.sha256, out);
    }
    else if (strcmp(alg, OCI_HASH_ALG_SHA512) == 0) {
        if (expected_len != sizeof(expected.sha512)) {
            *out = false;
            return -1;
        }
        return sha512_verify_file(path, expected.sha512, out);
    }
    else if (strcmp(alg, OCI_HASH_ALG_BLAKE3) == 0) {
        if (expected_len != sizeof(expected.blake3)) {
            *out = false;
            return -1;
        }
        return blake3_verify_file(path, expected.blake3, out);
    }
    else {
        fprintf(stderr, "[x] Unsupported hash algorithm: %s\n", alg);
        return -1;
    }
}

static int nsh_oci_verify_blob(nsh_oci_import_blob_t *blob, bool *out)
{
    char alg[NAME_MAX + 1];
    char encoded[NAME_MAX + 1];
    int err;

    err = nsh_oci_digest_parse(blob->digest, alg, sizeof(alg), encoded, sizeof(encoded));
    if (err)
        return -1;

    err = nsh_oci_verify_blob_hash(blob->source_path, alg, encoded, out);
    if (err) {
        fprintf(stderr, "[x] Failed to verify hash of blob: %s\n", alg);
        return -1;
    }

    return 0;
}

static int nsh_oci_import_layer(nsh_oci_import_blob_t *blob)
{
    int archive_format = nsh_oci_layer_archive_format(blob->media_type);
    int archive_filter = nsh_oci_layer_archive_filter(blob->media_type);

    if (archive_format == -1 || archive_filter == -1) {
        fprintf(stderr,
            "[x] Unsupported layer media type: %s\n",
            blob->media_type);
        return -1;
    }

    blob->size = nsh_oci_layer_unpack(
        blob->source_path, blob->target_path,
        archive_format, archive_filter);
    if (blob->size == -1)
        return -1;

    return 0;
}

static int nsh_oci_import_meta(nsh_oci_import_blob_t *blob)
{
    int err;

    err = file_copy(blob->source_path, blob->target_path);
    if (err) {
        perror("[x] Failed to copy blob");
        return -1;
    }

    blob->size = file_size(blob->target_path);
    if (blob->size == -1) {
        perror("[x] Failed to read blob size");
        return -1;
    }

    return 0;
}

static int nsh_oci_import_blob_data(nsh_oci_import_blob_t *blob)
{
    int err;

    if (blob->type == NSH_OCI_BLOB_LAYER) {
        err = nsh_oci_import_layer(blob);
        if (err) {
            fprintf(stderr, "Failed to import layer\n");
            return -1;
        }
    }
    else {
        err = nsh_oci_import_meta(blob);
        if (err) {
            fprintf(stderr, "Failed to import metadata\n");
            return -1;
        }
    }

    return 0;
}

static int nsh_oci_import_blob_single(nsh_oci_import_context_t *ctx,
    const char *digest, const char *media_type)
{
    nsh_oci_import_blob_t blob;
    bool is_verified;
    int err;

    err = nsh_oci_import_blob_init(&blob, ctx, digest, media_type);
    if (err)
        return -1;

    err = nsh_oci_verify_blob(&blob, &is_verified);
    if (err || !is_verified) {
        fprintf(stderr, "[x] Failed to verify integrity of blob with digest: %s\n", digest);
        return -1;
    }

    if (access(blob.target_path, F_OK) == 0) {
        printf("[*] Blob: %s already exists, skipping...\n", digest);

        blob.size = nsh_oci_blob_size_query(ctx->db, digest);
        if (blob.size == -1)
            return -1;
    }
    else {
        err = nsh_oci_import_blob_data(&blob);
        if (err)
            return -1;
    }

    err = nsh_oci_blob_commit(ctx->imported, &blob);
    if (err)
        return -1;

    return 0;
}

static int nsh_oci_import_blobs(nsh_oci_import_context_t *ctx)
{
    int err;

    err = nsh_oci_store_create(ctx->store_path);
    if (err)
        return -1;

    for (size_t i = 0; i < ctx->meta.manifest->layers_len; i++) {
        err = nsh_oci_import_blob_single(ctx,
            ctx->meta.manifest->layers[i]->digest,
            ctx->meta.manifest->layers[i]->media_type);

        if (err)
            return -1;
    }

    err = nsh_oci_import_blob_single(ctx,
        ctx->meta.manifest->config->digest, OCI_MEDIA_TYPE_CONFIG);
    if (err)
        return -1;

    err = nsh_oci_import_blob_single(ctx, ctx->desc->ref, OCI_MEDIA_TYPE_MANIFEST);
    if (err)
        return -1;

    ctx->desc->size = nsh_oci_image_size_calc(ctx->imported);

    return 0;
}

static int nsh_oci_import_persist(nsh_oci_import_context_t *ctx)
{
    nsh_oci_import_blob_t *blob;
    int refcount;

    for (size_t i = 0; i < cc_array_size(ctx->imported); i++) {
        cc_array_get_at(ctx->imported, i, (void **)&blob);

        refcount = nsh_blobs_repo_acquire(ctx->db, blob->digest, blob->size);
        if (refcount == -1)
            return -1;
    }

    return 0;
}

int nsh_oci_image_import(
    const char *store_path, const char *bundle_path,
    nsh_oci_image_desc_t *desc)
{
    nsh_oci_import_context_t ctx;
    int err;
    int ret = 0;

    err = nsh_oci_import_context_init(&ctx, store_path, bundle_path, desc);
    if (err) {
        ret = -1;
        goto out;
    }

    err = nsh_oci_import_blobs(&ctx);
    if (err) {
        ret = -1;
        goto out_destroy_ctx;
    }

    err = nsh_oci_import_persist(&ctx);
    if (err) {
        ret = -1;
        goto out_destroy_ctx;
    }

out_destroy_ctx:
    nsh_oci_import_context_destroy(&ctx);
out:
    return ret;
}

static int nsh_oci_image_export_blobs(nsh_oci_export_context_t *ctx)
{
    int err;

    err = nsh_oci_bundle_create(ctx->bundle_path);
    if (err)
        return -1;

    err = nsh_oci_bundle_dump_image_meta(&ctx->meta, ctx->bundle_path);
    if (err)
        return -1;

    err = nsh_oci_bundle_seal(ctx->bundle_path, ctx->ref);
    if (err)
        return -1;

    return 0;
}

int nsh_oci_image_export(
    const char *store_path, const char *ref, const char *bundle_path)
{
    nsh_oci_export_context_t ctx;
    int err;
    int ret = 0;

    err = nsh_oci_export_context_init(&ctx, store_path, ref, bundle_path);
    if (err)
        goto out;

    err = nsh_oci_image_export_blobs(&ctx);
    if (err)
        goto out_destroy_ctx;

out_destroy_ctx:
    nsh_oci_export_context_destroy(&ctx);
out:
    return ret;
}

static int nsh_oci_release_blob_single(nsh_database_t *db,
    const char *store_path, const char *digest, nsh_oci_blob_type_t type)
{
    char path[PATH_MAX];
    int refcount;
    int err;

    refcount = nsh_blobs_repo_release(db, digest);
    if (refcount == -1)
        return -1;
    if (refcount > 0)
        return 0;

    nsh_oci_blob_path_resolve(
        store_path, digest, type, path, sizeof(path));

    err = rmdir_recursive(path);
    if (err) {
        perror("[x] Failed to delete blob");
        return -1;
    }

    return 0;
}

static int nsh_oci_release_blobs(
    nsh_database_t *db, nsh_oci_image_meta_t *meta, const char *ref, const char *store_path)
{
    int err;

    err = nsh_oci_release_blob_single(db, store_path, ref, NSH_OCI_BLOB_MANIFEST);
    if (err)
        return -1;

    err = nsh_oci_release_blob_single(db,
        store_path, meta->manifest->config->digest, NSH_OCI_BLOB_CONFIG);
    if (err)
        return -1;

    for (size_t i = 0; i < meta->manifest->layers_len; i++) {
        err = nsh_oci_release_blob_single(db,
            store_path, meta->manifest->layers[i]->digest, NSH_OCI_BLOB_LAYER);
        if (err)
            return -1;
    }

    return 0;
}

int nsh_oci_image_delete(const char *store_path, const char *ref)
{
    nsh_database_t *db;
    nsh_oci_image_meta_t meta;
    int err;
    int ret = 0;

    db = nsh_database_open();
    if (db == NULL) {
        ret = -1;
        goto out;
    }

    err = nsh_oci_store_load_image_meta(&meta, store_path, ref);
    if (err) {
        ret = -1;
        goto out_close_db;
    }

    err = nsh_oci_release_blobs(db, &meta, ref, store_path);
    if (err) {
        ret = -1;
        goto out_destroy_meta;
    }

out_destroy_meta:
    nsh_oci_image_meta_destroy(&meta);
out_close_db:
    nsh_database_close(db);
out:
    return ret;
}

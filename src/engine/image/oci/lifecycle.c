#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "common/utils/hashing.h"
#include "common/utils/string.h"
#include "common/utils/filesystem.h"
#include "engine/persistence/database.h"
#include "engine/persistence/repos/blobs.h"
#include "engine/image/oci/common.h"
#include "engine/image/oci/parse.h"
#include "engine/image/oci/bundle.h"
#include "engine/image/oci/store.h"
#include "engine/image/oci/unpack.h"
#include "engine/image/oci/lifecycle.h"

typedef struct {
    const char *digest;
    const char *media_type;
    nsh_oci_blob_type_t blob_type;
    char blob_path[PATH_MAX];
    char dest_path[PATH_MAX];
} nsh_oci_blob_import_context_t;

static nsh_oci_blob_type_t nsh_oci_classify_blob(const char *media_type)
{
    if (strcmp(media_type, OCI_MEDIA_TYPE_MANIFEST) == 0)
        return NSH_OCI_BLOB_MANIFEST;
    if (strcmp(media_type, OCI_MEDIA_TYPE_CONFIG) == 0)
        return NSH_OCI_BLOB_CONFIG;
    return NSH_OCI_BLOB_LAYER;
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

static int nsh_oci_verify_blob(const char *path, const char *digest, bool *out)
{
    char alg[NAME_MAX + 1];
    char encoded[NAME_MAX + 1];
    int err;

    err = nsh_oci_digest_parse(digest, alg, sizeof(alg), encoded, sizeof(encoded));
    if (err)
        return -1;

    err = nsh_oci_verify_blob_hash(path, alg, encoded, out);
    if (err) {
        fprintf(stderr, "[x] Failed to verify hash of blob: %s\n", alg);
        return -1;
    }

    return 0;
}

static int nsh_oci_import_layer(nsh_oci_blob_import_context_t *ctx)
{
    int archive_format = nsh_oci_layer_archive_format(ctx->media_type);
    int archive_filter = nsh_oci_layer_archive_filter(ctx->media_type);
    int err;

    if (archive_format == -1 || archive_filter == -1) {
        fprintf(stderr, "[x] Unsupported layer media type: %s\n", ctx->media_type);
        return -1;
    }

    err = nsh_oci_layer_unpack(
        ctx->blob_path, ctx->dest_path, archive_format, archive_filter);
    if (err)
        return -1;

    return 0;
}

static int nsh_oci_import_blob_data(nsh_oci_blob_import_context_t *ctx)
{
    int err;

    if (ctx->blob_type == NSH_OCI_BLOB_LAYER) {
        err = nsh_oci_import_layer(ctx);
        if (err) {
            fprintf(stderr, "Failed to import layer\n");
            return -1;
        }
    }
    else {
        err = copy_file(ctx->blob_path, ctx->dest_path);
        if (err) {
            perror("[x] Failed to copy blob");
            return -1;
        }
    }

    return 0;
}

static int nsh_oci_import_blob_single(
    const char *store_path, const char *bundle_path,
    const char *digest, const char *media_type,
    struct cc_array_s *imported)
{
    enum cc_stat cc_stat;
    nsh_oci_blob_import_context_t ctx;
    bool is_verified;
    int err;

    ctx.blob_type = nsh_oci_classify_blob(media_type);
    ctx.digest = digest;
    ctx.media_type = media_type;

    nsh_oci_store_blob_path_resolve(
        store_path, digest, ctx.blob_type, ctx.dest_path, sizeof(ctx.dest_path));

    err = nsh_oci_bundle_blob_path_resolve(
        bundle_path, digest, ctx.blob_path, sizeof(ctx.blob_path));
    if (err)
        return -1;

    err = nsh_oci_verify_blob(ctx.blob_path, digest, &is_verified);
    if (err || !is_verified) {
        fprintf(stderr, "[x] Failed to verify integrity of blob with digest: %s\n", digest);
        return -1;
    }

    if (access(ctx.dest_path, F_OK) == 0)
        printf("[*] Blob: %s already exists, skipping...\n", digest);
    else {
        err = nsh_oci_import_blob_data(&ctx);
        if (err)
            return -1;
    }

    cc_stat = cc_array_add(imported, (void *)digest);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to add layer digest: %s\n", digest);
        return -1;
    }

    return 0;
}

static int nsh_oci_import_blobs(
    const char *store_path, const char *bundle_path,
    const char *ref, nsh_oci_image_meta_t *meta,
    struct cc_array_s *digests)
{
    int err;

    err = nsh_oci_store_create(store_path);
    if (err)
        return -1;

    for (size_t i = 0; i < meta->manifest->layers_len; i++) {
        err = nsh_oci_import_blob_single(
            store_path, bundle_path,
            meta->manifest->layers[i]->digest,
            meta->manifest->layers[i]->media_type, digests);

        if (err)
            return -1;
    }

    err = nsh_oci_import_blob_single(store_path, bundle_path,
        meta->manifest->config->digest, OCI_MEDIA_TYPE_CONFIG, digests);
    if (err)
        return -1;

    err = nsh_oci_import_blob_single(store_path, bundle_path,
        ref, OCI_MEDIA_TYPE_MANIFEST, digests);
    if (err)
        return -1;

    return 0;
}

static int nsh_oci_import_persist(struct cc_array_s *digests)
{
    nsh_database_t *db;
    char *digest;
    int refcount;
    int ret = 0;

    db = nsh_database_open();
    if (db == NULL) {
        ret = -1;
        goto out;
    }

    for (size_t i = 0; i < cc_array_size(digests); i++) {
        cc_array_get_at(digests, i, (void **)&digest);

        refcount = nsh_blobs_repo_acquire(db, digest);
        if (refcount == -1) {
            ret = -1;
            goto out_close_db;
        }
    }

out_close_db:
    nsh_database_close(db);
out:
    return ret;
}

int nsh_oci_image_import(const char *store_path, const char *bundle_path, char **out)
{
    struct cc_array_s *digests;
    enum cc_stat cc_stat;
    nsh_oci_image_meta_t meta;
    char *ref;
    int err;
    int ret = 0;

    cc_stat = cc_array_new(&digests);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate digests array\n");
        ret = -1;
        goto out;
    }

    err = nsh_oci_bundle_load_image_meta(&meta, bundle_path, &ref);
    if (err) {
        ret = -1;
        goto out_destroy_digests;
    }

    err = nsh_oci_import_blobs(store_path, bundle_path, ref, &meta, digests);
    if (err) {
        ret = -1;
        goto out_destroy_meta;
    }

    err = nsh_oci_import_persist(digests);
    if (err) {
        ret = -1;
        goto out_destroy_meta;
    }

    *out = ref;

out_destroy_meta:
    nsh_oci_image_meta_destroy(&meta);
out_destroy_digests:
    cc_array_destroy(digests);
out:
    return ret;
}

static int nsh_oci_release_blob_single(nsh_database_t *db,
    const char *store_path, const char *digest, nsh_oci_blob_type_t type)
{
    char path[PATH_MAX];
    int refcount;
    int err;

    // TODO: fix race conditions

    refcount = nsh_blobs_repo_release(db, digest);
    if (refcount == -1)
        return -1;
    if (refcount > 0)
        return 0;

    nsh_oci_store_blob_path_resolve(
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

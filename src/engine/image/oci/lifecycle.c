#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "common/constants.h"
#include "common/utils/array.h"
#include "common/utils/hashing.h"
#include "common/utils/string.h"
#include "common/utils/filesystem.h"
#include "common/utils/macros.h"
#include "engine/image/oci/parse.h"
#include "engine/image/oci/bundle.h"
#include "engine/image/oci/store.h"
#include "engine/image/oci/unpack.h"
#include "engine/image/oci/lifecycle.h"

#define OCI_HASH_ALG_SHA256 "sha256"
#define OCI_HASH_ALG_SHA512 "sha512"
#define OCI_HASH_ALG_BLAKE3 "blake3"

void nsh_oci_lc_context_destroy(nsh_oci_lc_context_t *ctx)
{
    free(ctx->ref);
    ARRAY_FREE_DEEP(ctx->blob_digests);
}

static int nsh_oci_image_store_create_if_missing()
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

static void nsh_oci_image_store_init(nsh_oci_image_store_t *store)
{
    store->snapshots_repo_path = NSH_STORE_SNAPSHOTS;
    store->manifests_repo_path = NSH_STORE_MANIFESTS;
    store->config_repo_path = NSH_STORE_CONFIG;
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

static int nsh_oci_import_blob(
    const char *repo_path, const char *bundle_path,
    const char *digest, struct cc_array_s *digests)
{
    char blob_path[PATH_MAX];
    char dest_path[PATH_MAX];
    bool is_verified;
    enum cc_stat cc_stat;
    int err;

    err = nsh_oci_bundle_blob_path_resolve(
        bundle_path, digest, blob_path, sizeof(blob_path));
    if (err)
        return -1;

    nsh_oci_store_blob_path_resolve(
        repo_path, digest, dest_path, sizeof(dest_path));

    err = nsh_oci_verify_blob(blob_path, digest, &is_verified);
    if (err || !is_verified) {
        fprintf(stderr, "[x] Failed to verify integrity of blob with digest: %s\n", digest);
        return -1;
    }

    cc_stat = cc_array_add_string(digests, digest);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to add blob digest: %s\n", digest);
        return -1;
    }

    if (access(dest_path, F_OK) == 0) {
        printf("[*] Blob: %s already exists, skipping...\n", digest);
        return 0;
    }

    err = copy_file(blob_path, dest_path);
    if (err) {
        perror("[x] Failed to copy blob");
        return -1;
    }

    return 0;
}

static int nsh_oci_import_layer_unpack(
    const char *dest_path, const char *layer_path,
    image_spec_schema_content_descriptor *layer)
{
    int archive_format = nsh_oci_layer_archive_format(layer);
    int archive_filter = nsh_oci_layer_archive_filter(layer);
    int err;

    if (archive_format == -1 || archive_filter == -1) {
        fprintf(stderr, "[x] Unsupported layer media type: %s\n",
            layer->media_type);
        return -1;
    }

    err = nsh_oci_layer_unpack(layer_path, dest_path, archive_format, archive_filter);
    if (err)
        return -1;

    return 0;
}

static int nsh_oci_import_layer(
    const char *repo_path, const char *bundle_path,
    image_spec_schema_content_descriptor *layer_descriptor,
    struct cc_array_s *digests)
{
    char *digest = layer_descriptor->digest;
    char layer_path[PATH_MAX];
    char dest_path[PATH_MAX];
    bool is_verified;
    enum cc_stat cc_stat;
    int err;

    err = nsh_oci_bundle_blob_path_resolve(
        bundle_path, digest, layer_path, sizeof(layer_path));
    if (err)
        return -1;

    nsh_oci_store_blob_path_resolve(
        repo_path, digest, dest_path, sizeof(dest_path));

    err = nsh_oci_verify_blob(layer_path, digest, &is_verified);
    if (err || !is_verified) {
        fprintf(stderr,
            "[x] Failed to verify integrity of layer with digest: %s\n", digest);
        return -1;
    }

    cc_stat = cc_array_add_string(digests, digest);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to add layer digest: %s\n", digest);
        return -1;
    }

    if (access(dest_path, F_OK) == 0) {
        printf("[*] Layer: %s already exists, skipping...\n", digest);
        return 0;
    }

    err = nsh_oci_import_layer_unpack(dest_path, layer_path, layer_descriptor);
    if (err)
        return -1;

    return 0;
}

static int nsh_oci_import_content(const char *bundle_path,
    nsh_oci_image_meta_t *meta, nsh_oci_image_store_t *store,
    nsh_oci_lc_context_t *ctx)
{
    struct cc_array_s *digests;
    enum cc_stat cc_stat;
    int err;

    cc_stat = cc_array_new(&digests);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate digests dynarray\n");
        goto error;
    }

    for (size_t i = 0; i < meta->manifest->layers_len; i++) {
        err = nsh_oci_import_layer(
            store->snapshots_repo_path, bundle_path,
            meta->manifest->layers[i], digests);
        if (err)
            goto error_destroy_digests;
    }

    err = nsh_oci_import_blob(
        store->manifests_repo_path, bundle_path,
        ctx->ref, digests);
    if (err)
        goto error_destroy_digests;

    err = nsh_oci_import_blob(
        store->config_repo_path, bundle_path,
        meta->manifest->config->digest, digests);
    if (err)
        goto error_destroy_digests;

    cc_stat = cc_array_to_static_shallow(digests,
        (void ***)&ctx->blob_digests, &ctx->blob_digests_len);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate digests array\n");
        goto error_destroy_digests;
    }

    cc_array_destroy(digests);

    return 0;

error_destroy_digests:
    cc_array_destroy_cb(digests, free);
error:
    return -1;
}

int nsh_oci_image_import(const char *bundle_path, nsh_oci_lc_context_t *ctx)
{
    nsh_oci_image_meta_t meta;
    nsh_oci_image_store_t store;
    int err;
    int ret = 0;

    nsh_oci_image_store_init(&store);

    err = nsh_oci_image_store_create_if_missing();
    if (err) {
        ret = -1;
        goto out;
    }

    err = nsh_oci_bundle_load_image_meta(&meta, bundle_path, &ctx->ref);
    if (err) {
        ret = -1;
        goto out;
    }

    err = nsh_oci_import_content(bundle_path, &meta, &store, ctx);
    if (err) {
        ret = -1;
        goto out_destroy_meta_and_ref;
    }

out_destroy_meta_and_ref:
    nsh_oci_image_meta_destroy(&meta);
out:
    return ret;
}

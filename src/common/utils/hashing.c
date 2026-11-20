#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sodium.h>
#include <blake3.h>
#include "common/utils/hashing.h"

#define MULTIPART_HASH_BUF_SIZE 4096

int sha256_verify_stream(FILE *stream,
    uint8_t expected[crypto_hash_sha256_BYTES], bool *out)
{
    crypto_hash_sha256_state state;
    uint8_t computed[crypto_hash_sha256_BYTES];
    uint8_t buf[MULTIPART_HASH_BUF_SIZE];
    ssize_t bytes_read;

    crypto_hash_sha256_init(&state);

    while ((bytes_read = fread(buf, 1, sizeof(buf), stream)) > 0)
        crypto_hash_sha256_update(&state, buf, bytes_read);

    if (ferror(stream)) {
        perror("[x] Failed to read from stream");
        return -1;
    }

    crypto_hash_sha256_final(&state, computed);
    *out = sodium_memcmp(computed, expected, sizeof(computed)) == 0;

    return 0;
}

int sha256_verify_file(const char *path,
    uint8_t expected[crypto_hash_sha256_BYTES], bool *out)
{
    FILE *stream;
    int err;
    int ret = 0;

    stream = fopen(path, "r");
    if (stream == NULL) {
        perror("[x] Failed to verify file SHA-256 digest");
        ret = -1;
        goto out;
    }

    err = sha256_verify_stream(stream, expected, out);
    if (err) {
        ret = -1;
        goto out_close_stream;
    }

out_close_stream:
    fclose(stream);
out:
    return ret;
}

int sha512_verify_stream(FILE *stream,
    uint8_t expected[crypto_hash_sha512_BYTES], bool *out)
{
    crypto_hash_sha512_state state;
    uint8_t computed[crypto_hash_sha512_BYTES];
    uint8_t buf[MULTIPART_HASH_BUF_SIZE];
    ssize_t bytes_read;

    crypto_hash_sha512_init(&state);

    while ((bytes_read = fread(buf, 1, sizeof(buf), stream)) > 0)
        crypto_hash_sha512_update(&state, buf, bytes_read);

    if (ferror(stream)) {
        perror("[x] Failed to read from stream");
        return -1;
    }

    crypto_hash_sha512_final(&state, computed);
    *out = sodium_memcmp(computed, expected, sizeof(computed)) == 0;

    return 0;
}

int sha512_verify_file(const char *path,
    uint8_t expected[crypto_hash_sha512_BYTES], bool *out)
{
    FILE *stream;
    int err;
    int ret = 0;

    stream = fopen(path, "r");
    if (stream == NULL) {
        perror("[x] Failed to verify file SHA-512 digest");
        ret = -1;
        goto out;
    }

    err = sha512_verify_stream(stream, expected, out);
    if (err) {
        ret = -1;
        goto out_close_stream;
    }

out_close_stream:
    fclose(stream);
out:
    return ret;
}

int blake3_verify_stream(FILE *stream, uint8_t expected[BLAKE3_OUT_LEN], bool *out)
{
    blake3_hasher hasher;
    uint8_t computed[BLAKE3_OUT_LEN];
    uint8_t buf[MULTIPART_HASH_BUF_SIZE];
    ssize_t bytes_read;

    blake3_hasher_init(&hasher);

    while ((bytes_read = fread(buf, 1, sizeof(buf), stream)) > 0)
        blake3_hasher_update(&hasher, buf, bytes_read);

    if (ferror(stream)) {
        perror("[x] Failed to read from stream");
        return -1;
    }

    blake3_hasher_finalize(&hasher, computed, sizeof(computed));
    *out = sodium_memcmp(computed, expected, sizeof(computed)) == 0;

    return 0;
}

int blake3_verify_file(const char *path,
    uint8_t expected[BLAKE3_OUT_LEN], bool *out)
{
    FILE *stream;
    int err;
    int ret = 0;

    stream = fopen(path, "r");
    if (stream == NULL) {
        perror("[x] Failed to verify file BLAKE3 digest");
        ret = -1;
        goto out;
    }

    err = blake3_verify_stream(stream, expected, out);
    if (err) {
        ret = -1;
        goto out_close_stream;
    }

out_close_stream:
    fclose(stream);
out:
    return ret;
}

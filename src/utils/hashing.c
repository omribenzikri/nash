#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sodium.h>
#include <blake3.h>
#include "utils/hashing.h"

#define NSH_MULTIPART_HASH_BUF_SIZE 4096

int nsh_verify_sha256_stream(FILE *stream, uint8_t expected[crypto_hash_sha256_BYTES], bool *out)
{
    crypto_hash_sha256_state state;
    uint8_t computed[crypto_hash_sha256_BYTES];
    uint8_t buf[NSH_MULTIPART_HASH_BUF_SIZE];
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

int nsh_verify_sha512_stream(FILE *stream, uint8_t expected[crypto_hash_sha512_BYTES], bool *out)
{
    crypto_hash_sha512_state state;
    uint8_t computed[crypto_hash_sha512_BYTES];
    uint8_t buf[NSH_MULTIPART_HASH_BUF_SIZE];
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

int nsh_verify_blake3_stream(FILE *stream, uint8_t expected[BLAKE3_OUT_LEN], bool *out)
{
    blake3_hasher hasher;
    uint8_t computed[BLAKE3_OUT_LEN];
    uint8_t buf[NSH_MULTIPART_HASH_BUF_SIZE];
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

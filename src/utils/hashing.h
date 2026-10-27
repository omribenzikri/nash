#ifndef _NASH_UTILS_HASHING_H
#define _NASH_UTILS_HASHING_H

#include <stdio.h>
#include <stdbool.h>
#include <sodium.h>
#include <blake3.h>

// Currently unused since verifying blob digests significantly slows down container creation.
// May be used in the future before inserting blobs to an image store, idk.
int nsh_verify_sha256_stream(FILE *stream, uint8_t expected[crypto_hash_sha256_BYTES], bool *out);
int nsh_verify_sha512_stream(FILE *stream, uint8_t expected[crypto_hash_sha512_BYTES], bool *out);
int nsh_verify_blake3_stream(FILE *stream, uint8_t expected[BLAKE3_OUT_LEN], bool *out);

#endif

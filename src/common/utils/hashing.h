#ifndef _NASH_COMMON_UTILS_HASHING_H
#define _NASH_COMMON_UTILS_HASHING_H

#include <stdio.h>
#include <stdbool.h>
#include <sodium.h>
#include <blake3.h>

int sha256_verify_stream(FILE *stream, uint8_t expected[crypto_hash_sha256_BYTES], bool *out);
int sha256_verify_file(const char *path, uint8_t expected[crypto_hash_sha256_BYTES], bool *out);

int sha512_verify_stream(FILE *stream, uint8_t expected[crypto_hash_sha512_BYTES], bool *out);
int sha512_verify_file(const char *path, uint8_t expected[crypto_hash_sha512_BYTES], bool *out);

int blake3_verify_stream(FILE *stream, uint8_t expected[BLAKE3_OUT_LEN], bool *out);
int blake3_verify_file(const char *path, uint8_t expected[BLAKE3_OUT_LEN], bool *out);

#endif

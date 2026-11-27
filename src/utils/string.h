#ifndef _NASH_UTILS_STRING_H
#define _NASH_UTILS_STRING_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

int parse_uintmax(const char *str, int base, uintmax_t max, uintmax_t *out);
ssize_t parse_hex(const char *str, uint8_t *buf, size_t size);

void format_bytes(uint64_t bytes, char *buf, size_t size);

#endif

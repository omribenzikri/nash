#ifndef _NASH_COMMON_UTILS_STRING_H
#define _NASH_COMMON_UTILS_STRING_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

int str_parse_uintmax(const char *str, int base, uintmax_t max, uintmax_t *out);
ssize_t str_parse_hex(const char *str, uint8_t *buf, size_t size);

#endif

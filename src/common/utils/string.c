#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include "common/utils/string.h"

int str_parse_uintmax(const char *str, int base, uintmax_t max, uintmax_t *out)
{
    char *end;
    uintmax_t value;

    if (str == NULL || *str == '\0' || *str == '-')
        return -1;

    errno = 0;
    value = strtoumax(str, &end, base);

    if (errno == ERANGE || end == str || *end != '\0' || value > max)
        return -1;

    *out = value;
    return 0;
}

ssize_t str_parse_hex(const char *str, uint8_t *buf, size_t size)
{
    size_t hex_len = strlen(str);
    char byte_hex[3];
    int err;

    if (hex_len % 2 != 0 || size < hex_len / 2)
        return -1;

    for (size_t i = 0; i < hex_len; i += 2) {
        byte_hex[0] = str[i];
        byte_hex[1] = str[i + 1];
        byte_hex[2] = '\0';

        err = str_parse_uintmax(byte_hex, 16, UINT8_MAX,
            (uintmax_t *)&buf[i / 2]);
        if (err)
            return -1;
    }

    return hex_len / 2;
}

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include "common/utils/string.h"

int str_parse_uintmax(const char *str, uintmax_t max, uintmax_t *out)
{
    char *end;
    uintmax_t value;

    if (str == NULL || *str == '\0' || *str == '-')
        return -1;

    errno = 0;
    value = strtoumax(str, &end, 10);

    if (errno == ERANGE || end == str || *end != '\0' || value > max)
        return -1;

    *out = value;
    return 0;
}

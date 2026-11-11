#ifndef _NASH_UTILS_MACROS_H
#define _NASH_UTILS_MACROS_H

#define ARRAY_WITH_LEN(type, name) type *name; size_t name##_len;

#define ARRAY_ITESM_FREE(arr)   \
do {                            \
    typeof(arr) ptr = arr;      \
    while (*ptr != NULL)        \
        free(*ptr++);           \
} while (0)

#endif

#ifndef _NASH_UTILS_MACROS_H
#define _NASH_UTILS_MACROS_H

#define ARRAY_WITH_LEN(type, name) type *name; size_t name##_len;

#define ARRAY_FREE_ITEMS(arr) \
do { typeof(arr) ptr = arr; while (*ptr != NULL) free(*ptr++); } while (0)

#define ARRAY_FREE_DEEP(arr) \
do { ARRAY_FREE_ITEMS(arr); free(arr); } while (0)

#endif

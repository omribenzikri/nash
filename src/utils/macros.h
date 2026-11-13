#ifndef _NASH_UTILS_MACROS_H
#define _NASH_UTILS_MACROS_H

#define ARRAY_WITH_LEN(type, name) type *name; size_t name##_len;

#define ARRAY_FREE_ITEMS(name) \
do { for (size_t i = 0; i < name##_len; i++) free(name[i]); } while (0)

#define ARRAY_FREE_DEEP(name) \
do { ARRAY_FREE_ITEMS(name); free(name); } while (0)

#endif

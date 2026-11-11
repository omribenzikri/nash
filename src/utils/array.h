#ifndef _NASH_UTILS_ARRAY_H
#define _NASH_UTILS_ARRAY_H

#include <stddef.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>

enum cc_stat cc_array_to_static_shallow(CC_Array *ar, void ***out);

enum cc_stat cc_array_add_string(CC_Array *ar, const char *str);
enum cc_stat cc_array_extend_strings(CC_Array *ar, char **src, size_t len);

#endif

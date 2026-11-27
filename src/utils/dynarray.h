#ifndef _NASH_UTILS_DYNARRAY_H
#define _NASH_UTILS_DYNARRAY_H

#include <stddef.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>

enum cc_stat cc_array_to_static_shallow(
    CC_Array *ar, void ***outp, size_t *outl);

enum cc_stat cc_array_to_static_flat(
    CC_Array *ar, size_t elsize, void **outp, size_t *outl);

enum cc_stat cc_array_add_deep(CC_Array *ar, void *element, void *(*cp)(void *));
enum cc_stat cc_array_add_deep_memcpy(CC_Array *ar, void *element, size_t size);

enum cc_stat cc_array_add_multiple_deep(
    CC_Array *ar, void **elements, size_t len, void *(*cp)(void *));

#endif

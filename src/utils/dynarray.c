#include <stdlib.h>
#include <stddef.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "utils/dynarray.h"

enum cc_stat cc_array_to_static_shallow(
    CC_Array *ar, void ***outp, size_t *outl)
{
    size_t size;
    void **buf;

    size = cc_array_size(ar);
    buf = malloc(size * sizeof(*buf));
    if (buf == NULL)
        return CC_ERR_ALLOC;

    for (size_t i = 0; i < size; i++)
        cc_array_get_at(ar, i, &buf[i]);

    *outp = buf;
    *outl = size;

    return CC_OK;
}

enum cc_stat cc_array_to_static_flat(
    CC_Array *ar, size_t elsize, void **outp, size_t *outl)
{
    size_t size;
    void *element;
    void *buf;

    size = cc_array_size(ar);
    buf = malloc(size * elsize);
    if (buf == NULL)
        return CC_ERR_ALLOC;

    for (size_t i = 0; i < size; i++) {
        cc_array_get_at(ar, i, &element);
        memcpy(buf + (i * elsize), element, elsize);
    }

    *outp = buf;
    *outl = size;

    return CC_OK;
}

enum cc_stat cc_array_add_deep(CC_Array *ar, void *element, void *(*cp)(void *))
{
    void *copy;
    enum cc_stat stat;

    copy = cp(element);
    if (copy == NULL) {
        stat = CC_ERR_ALLOC;
        goto error;
    }

    stat = cc_array_add(ar, copy);
    if (stat != CC_OK)
        goto error_free_copy;

    return CC_OK;

error_free_copy:
    free(copy);
error:
    return stat;
}

enum cc_stat cc_array_add_deep_memcpy(CC_Array *ar, void *element, size_t size)
{
    void *copy;
    enum cc_stat stat;

    copy = malloc(size);
    if (copy == NULL) {
        stat = CC_ERR_ALLOC;
        goto error;
    }
    memcpy(copy, element, size);

    stat = cc_array_add(ar, copy);
    if (stat != CC_OK)
        goto error_free_copy;

    return CC_OK;

error_free_copy:
    free(copy);
error:
    return stat;
}

enum cc_stat cc_array_add_multiple_deep(
    CC_Array *ar, void **elements, size_t len, void *(*cp)(void *))
{
    enum cc_stat stat;

    for (size_t i = 0; i < len; i++) {
        stat = cc_array_add_deep(ar, elements[i], cp);
        if (stat != CC_OK) {
            return stat;
        }
    }

    return CC_OK;
}

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "utils/array.h"

void **array_realloc_nullterm_shallow(void **ptr, size_t len)
{
    void **new;

    new = realloc(ptr, (len + 1) * sizeof(*ptr));
    if (new == NULL)
        return NULL;

    new[len] = NULL;
    return new;
}


enum cc_stat cc_array_to_static_shallow(
    CC_Array *ar, void ***outp, size_t *outl)
{
    size_t size;
    void **buf;

    size = cc_array_size(ar);
    buf = calloc(size, sizeof(*buf));
    if (buf == NULL)
        return CC_ERR_ALLOC;

    for (size_t i = 0; i < size; i++)
        cc_array_get_at(ar, i, &buf[i]);

    *outp = buf;
    *outl = size;

    return CC_OK;
}

enum cc_stat cc_array_to_static_values(
    CC_Array *ar, size_t elsize, void **outp, size_t *outl)
{
    size_t size;
    size_t offset;
    void *element;
    void *buf;

    size = cc_array_size(ar);
    buf = calloc(size, elsize);
    if (buf == NULL)
        return CC_ERR_ALLOC;

    for (size_t i = 0; i < size; i++) {
        offset = i * elsize;
        cc_array_get_at(ar, i, &element);
        memcpy(buf + offset, element, elsize);
    }

    *outp = buf;
    *outl = size;

    return CC_OK;
}

enum cc_stat cc_array_add_value(CC_Array *ar, void *val, size_t size)
{
    void *element;
    enum cc_stat stat;

    element = malloc(size);
    if (element == NULL) {
        stat = CC_ERR_ALLOC;
        goto error;
    }
    memcpy(element, val, size);

    stat = cc_array_add(ar, element);
    if (stat != CC_OK)
        goto error_free_element;

    return CC_OK;

error_free_element:
    free(element);
error:
    return stat;
}

enum cc_stat cc_array_add_string(CC_Array *ar, const char *str)
{
    char *clone;
    enum cc_stat stat;

    clone = strdup(str);
    if (clone == NULL) {
        stat = CC_ERR_ALLOC;
        goto error;
    }

    stat = cc_array_add(ar, clone);
    if (stat != CC_OK)
        goto error_free_clone;

    return CC_OK;

error_free_clone:
    free(clone);
error:
    return stat;
}

enum cc_stat cc_array_extend_strings(CC_Array *ar, char **src, size_t len)
{
    enum cc_stat stat;

    for (size_t i = 0; i < len; i++) {
        stat = cc_array_add_string(ar, src[i]);
        if (stat != CC_OK) {
            return stat;
        }
    }

    return CC_OK;
}

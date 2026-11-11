#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "utils/array.h"

enum cc_stat cc_array_to_static_shallow(CC_Array *ar, void ***out)
{
    size_t size;
    void **buf;

    size = cc_array_size(ar);
    buf = calloc(size + 1, sizeof(*buf));
    if (buf == NULL)
        return CC_ERR_ALLOC;

    for (size_t i = 0; i < size; i++)
        cc_array_get_at(ar, i, &buf[i]);

    buf[size] = NULL;
    *out = buf;

    return CC_OK;
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

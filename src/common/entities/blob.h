#ifndef _NASH_COMMON_ENTITIES_BLOB_H
#define _NASH_COMMON_ENTITIES_BLOB_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char *digest;
    int64_t size;
    size_t refcount;
} nsh_blob_entity_t;

nsh_blob_entity_t *nsh_blob_entity_new(const char *digest, int64_t size, size_t refcount);
void nsh_blob_entity_free(nsh_blob_entity_t *entity);

#endif

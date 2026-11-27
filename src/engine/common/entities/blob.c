#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "engine/common/entities/blob.h"

nsh_blob_entity_t *nsh_blob_entity_new(const char *digest, int64_t size, size_t refcount)
{
    nsh_blob_entity_t *entity;

    entity = malloc(sizeof(*entity));
    if (entity == NULL) {
        perror("[x] Failed to allocate blob entity");
        goto error;
    }

    entity->digest = strdup(digest);
    if (entity->digest == NULL) {
        perror("[x] Failed to allocate blob entity name");
        goto error_free_entity;
    }

    entity->size = size;
    entity->refcount = refcount;

    return entity;

error_free_entity:
    free(entity);
error:
    return NULL;
}

void nsh_blob_entity_free(nsh_blob_entity_t *entity)
{
    free(entity->digest);
    free(entity);
}

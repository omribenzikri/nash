#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <uuid/uuid.h>
#include "engine/common/entities/image.h"

nsh_image_entity_t *nsh_image_entity_new(
    const uuid_t id, const char *name, const char *ref,
    int64_t size, time_t created_at)
{
    nsh_image_entity_t *entity;

    entity = malloc(sizeof(*entity));
    if (entity == NULL) {
        perror("[x] Failed to allocate image entity");
        goto error;
    }

    uuid_copy(entity->id, id);

    entity->name = strdup(name);
    if (entity->name == NULL) {
        perror("[x] Failed to allocate image entity name");
        goto error_free_entity;
    }

    entity->ref = strdup(ref);
    if (entity->ref == NULL) {
        perror("[x] Failed to allocate image ref");
        goto error_free_name;
    }

    entity->size = size;
    entity->created_at = created_at;

    return entity;

error_free_name:
    free(entity->name);
error_free_entity:
    free(entity);
error:
    return NULL;
}

nsh_image_entity_t *nsh_image_entity_clone(nsh_image_entity_t *entity)
{
    return nsh_image_entity_new(
        entity->id, entity->name, entity->ref,
        entity->size, entity->created_at);
}

void nsh_image_entity_free(nsh_image_entity_t *entity)
{
    free(entity->name);
    free(entity->ref);
    free(entity);
}

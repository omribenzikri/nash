#ifndef _NASH_COMMON_ENTITIES_IMAGE_H
#define _NASH_COMMON_ENTITIES_IMAGE_H

#include <time.h>
#include <uuid/uuid.h>

typedef struct {
    uuid_t id;
    char *name;
    char *ref;
    time_t created_at;
} nsh_image_entity_t;

nsh_image_entity_t *nsh_image_entity_new(
    const uuid_t id, const char *name, const char *ref, const time_t created_at);
nsh_image_entity_t *nsh_image_entity_clone(nsh_image_entity_t *entity);
void nsh_image_entity_free(nsh_image_entity_t *entity);

#endif

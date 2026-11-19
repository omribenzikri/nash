#ifndef _NASH_ENGINE_PERSISTENCE_REPOS_IMAGES_H
#define _NASH_ENGINE_PERSISTENCE_REPOS_IMAGES_H

#include <stdbool.h>
#include <time.h>
#include <uuid/uuid.h>
#include "engine/persistence/database.h"

typedef struct {
    uuid_t id;
    char *name;
    char *ref;
    time_t created_at;
} nsh_image_entity_t;

nsh_image_entity_t *nsh_image_entity_alloc(
    const uuid_t id, const char *name, const char *ref, time_t created_at);
nsh_image_entity_t *nsh_image_entity_clone(nsh_image_entity_t *entity);
void nsh_image_entity_free(nsh_image_entity_t *entity);

int nsh_images_repo_create(nsh_database_t *db);

int nsh_images_repo_iter_by_name(
    nsh_database_t *db, const char *name,
    int (*cb)(nsh_image_entity_t *, void *), void *data);

int nsh_images_repo_add(nsh_database_t *db, nsh_image_entity_t *entity);
int nsh_images_repo_exists_by_name(nsh_database_t *db, const char *name, bool *out);
int nsh_images_repo_first_by_name(nsh_database_t *db, const char *name, nsh_image_entity_t **out);
int nsh_images_repo_delete_by_id(nsh_database_t *db, uuid_t id);

#endif

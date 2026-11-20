#ifndef _NASH_ENGINE_PERSISTENCE_REPOS_IMAGES_H
#define _NASH_ENGINE_PERSISTENCE_REPOS_IMAGES_H

#include <stdbool.h>
#include <uuid/uuid.h>
#include <collectc/cc_array.h>
#include "common/entities/image.h"
#include "engine/persistence/database.h"

int nsh_images_repo_create(nsh_database_t *db);

int nsh_images_repo_all(nsh_database_t *db, struct cc_array_s **records);
int nsh_images_repo_add(nsh_database_t *db, nsh_image_entity_t *entity);

int nsh_images_repo_exists_by_name(nsh_database_t *db, const char *name, bool *out);
int nsh_images_repo_first_by_name(nsh_database_t *db, const char *name, nsh_image_entity_t **out);

int nsh_images_repo_delete_by_id(nsh_database_t *db, uuid_t id);

#endif

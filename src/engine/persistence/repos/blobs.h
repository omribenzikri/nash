#ifndef _NASH_ENGINE_PERSISTENCE_REPOS_BLOBS_H
#define _NASH_ENGINE_PERSISTENCE_REPOS_BLOBS_H

#include <stdint.h>
#include "engine/common/entities/blob.h"
#include "engine/persistence/database.h"

int nsh_blobs_repo_create(nsh_database_t *db);

int nsh_blobs_repo_acquire(nsh_database_t *db, const char *digest, int64_t size);
int nsh_blobs_repo_release(nsh_database_t *db, const char *digest);

int nsh_blobs_repo_first_by_digest(
    nsh_database_t *db, const char *digest, nsh_blob_entity_t **out);

#endif

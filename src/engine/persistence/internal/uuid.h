#ifndef _NASH_ENGINE_PERSISTENCE_INTERNAL_UUID_H
#define _NASH_ENGINE_PERSISTENCE_INTERNAL_UUID_H

#include <uuid/uuid.h>
#include <sqlite3.h>
#include "utils/macros.h"

#define NSH_UUID_TYPE_LEN 16
#define NSH_UUID_TYPE "BLOB CHECK(length(id) = " STRINGIFY(NSH_UUID_TYPE_LEN) ")"

const unsigned char *nsh_column_uuid(sqlite3_stmt *stmt, int idx);
int nsh_bind_uuid(sqlite3_stmt *stmt, int idx, const uuid_t val, int n, void(*lifetime)(void*));

#endif

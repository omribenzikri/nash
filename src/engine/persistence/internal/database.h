#ifndef _NASH_ENGINE_PERSISTENCE_INTERNAL_DATABASE_H
#define _NASH_ENGINE_PERSISTENCE_INTERNAL_DATABASE_H

#include <sqlite3.h>
#include <uuid/uuid.h>
#include "common/utils/macros.h"

#define NSH_UUID_TYPE_LEN 16
#define NSH_UUID_TYPE "BLOB CHECK(length(id) = " STRINGIFY(NSH_UUID_TYPE_LEN) ")"

struct nsh_database {
    sqlite3 *conn;
};

const unsigned char *nsh_column_uuid(sqlite3_stmt *stmt, int idx);

#endif

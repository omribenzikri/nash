#include <uuid/uuid.h>
#include <sqlite3.h>
#include "engine/persistence/internal/uuid.h"

inline const unsigned char *nsh_column_uuid(sqlite3_stmt *stmt, int idx)
{
    return sqlite3_column_blob(stmt, idx);
}

inline int nsh_bind_uuid(sqlite3_stmt *stmt, int idx, const uuid_t val, int n, void(*lifetime)(void*))
{
    return sqlite3_bind_blob(stmt, idx, val, n, lifetime);
}

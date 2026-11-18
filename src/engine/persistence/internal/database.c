#include <sqlite3.h>
#include "engine/persistence/internal/database.h"

inline const unsigned char *nsh_column_uuid(sqlite3_stmt *stmt, int idx)
{
    return sqlite3_column_blob(stmt, idx);
}

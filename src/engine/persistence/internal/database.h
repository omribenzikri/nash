#ifndef _NASH_ENGINE_PERSISTENCE_INTERNAL_DATABASE_H
#define _NASH_ENGINE_PERSISTENCE_INTERNAL_DATABASE_H

#include <sqlite3.h>
#include <uuid/uuid.h>

struct nsh_database {
    sqlite3 *conn;
};

#endif

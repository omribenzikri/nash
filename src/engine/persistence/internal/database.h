#ifndef _NASH_ENGINE_PERSISTENCE_INTERNAL_DATABASE_H
#define _NASH_ENGINE_PERSISTENCE_INTERNAL_DATABASE_H

#include <sqlite3.h>
#include <uuid/uuid.h>

struct nsh_database {
    sqlite3 *conn;
};

typedef enum {
    NSH_DB_ITER_CONTINUE,
    NSH_DB_ITER_STOP,
    NSH_DB_ITER_ERR
} nsh_database_iter_cb_ret_t;

#endif

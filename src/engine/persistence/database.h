#ifndef _NASH_ENGINE_PERSISTENCE_DATABASE_H
#define _NASH_ENGINE_PERSISTENCE_DATABASE_H

typedef struct nsh_database nsh_database_t;

int nsh_database_create(void);

nsh_database_t *nsh_database_open(void);
void nsh_database_close(nsh_database_t *db);

#endif

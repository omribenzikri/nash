#ifndef _NASH_CONTAINER_H
#define _NASH_CONTAINER_H

#include <sys/types.h>
#include <uuid/uuid.h>

typedef struct {
    uuid_t id;
    pid_t pid;
} nsh_container_t;

#endif

#ifndef _NASH_RESOURCES_H
#define _NASH_RESOURCES_H

#include <uuid/uuid.h>

int nsh_create_cgroup(uuid_t container_id);
int nsh_delete_cgroup(uuid_t container_id);

#endif

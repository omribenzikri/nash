#ifndef _NASH_RESOURCES_H
#define _NASH_RESOURCES_H

#include <stdint.h>
#include <uuid/uuid.h>

typedef struct {
    float cpu_time;
    uint64_t memory_bytes;
} nsh_resources_t;

int nsh_create_cgroup(uuid_t container_id, nsh_resources_t *resources);
int nsh_delete_cgroup(uuid_t container_id);

#endif

#ifndef _NASH_CONTAINER_RESOURCES_H
#define _NASH_CONTAINER_RESOURCES_H

#include <stdint.h>
#include <uuid/uuid.h>

typedef struct {
    uint8_t cpu_time;
    uint32_t procs;
    uint64_t memory_bytes;
    uint64_t memory_swap_bytes;
} cntr_resources_t;

int cntr_cgroup_create(uuid_t id, cntr_resources_t *resources);
int cntr_cgroup_open(uuid_t id);
int cntr_cgroup_delete(uuid_t id);

#endif

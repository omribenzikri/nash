#ifndef _NASH_RUNTIME_RESOURCES_H
#define _NASH_RUNTIME_RESOURCES_H

#include <stdint.h>
#include <uuid/uuid.h>

typedef struct {
    uint8_t cpu_time;
    uint32_t procs;
    uint64_t memory_bytes;
    uint64_t memory_swap_bytes;
} nsh_resources_t;

int nsh_create_cgroup(uuid_t container_id, nsh_resources_t *resources);
int nsh_delete_cgroup(uuid_t container_id);

#endif

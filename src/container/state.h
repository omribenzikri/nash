#ifndef _NASH_CONTAINER_STATE_H
#define _NASH_CONTAINER_STATE_H

#include <stdint.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include "container/common.h"

typedef struct {
    uuid_t id;
    pid_t pid;
    uint64_t start_time;
    cntr_config_t config;
} cntr_state_t;

int cntr_state_dump(cntr_state_t *state, const char *workspace_path);

#endif

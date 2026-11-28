#ifndef _NASH_CONTAINER_CONTAINER_H
#define _NASH_CONTAINER_CONTAINER_H

#include <uuid/uuid.h>
#include "container/common.h"

int cntr_create(uuid_t id, cntr_config_t *config);
int cntr_delete(uuid_t id);

int cntr_settings_set_root(const char *path);

#endif

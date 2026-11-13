#ifndef _NASH_DAEMON_CONTAINER_H
#define _NASH_DAEMON_CONTAINER_H

#include "engine/container/container.h"

int nsh_container_create(nsh_container_params_t *params, nsh_container_t **out);
void nsh_container_delete(nsh_container_t *container);

#endif

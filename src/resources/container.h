#ifndef _NASH_RESOURCES_CONTAINER_H
#define _NASH_RESOURCES_CONTAINER_H

#include "engine/container.h"

int nsh_container_create(nsh_container_params_t *params, nsh_container_t **out);
void nsh_container_delete(nsh_container_t *container);

#endif

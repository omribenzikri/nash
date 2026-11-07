#include <stddef.h>
#include "engine/container.h"
#include "container.h"

int nsh_container_create(nsh_container_config_t *config, nsh_container_t **out)
{
    nsh_container_t *container;

    container = nsh_engine_container_create(config);
    if (container == NULL)
        return -1;

    *out = container;

    return 0;
}

void nsh_container_delete(nsh_container_t *container)
{
    nsh_engine_container_delete(container);
}

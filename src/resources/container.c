#include <stddef.h>
#include "engine/container.h"
#include "container.h"

int nsh_container_create(nsh_container_params_t *params, nsh_container_t **out)
{
    nsh_container_t *container;

    container = nsh_engine_container_create(params);
    if (container == NULL)
        return -1;

    *out = container;

    return 0;
}

void nsh_container_delete(nsh_container_t *container)
{
    nsh_engine_container_delete(container);
    nsh_engine_container_free(container);
}

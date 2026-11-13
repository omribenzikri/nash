#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "engine/container/config/env.h"

bool nsh_env_is_valid(const char *var)
{
    char *delim;
    size_t delim_idx;

    delim = strchr(var, '=');
    if (delim == NULL)
        return false;

    delim_idx = delim - var;

    return  0 < delim_idx && delim_idx < strlen(var);
}

bool nsh_env_cmp_key(const char *var1, const char *var2)
{
    char *delim1; size_t delim1_idx;
    char *delim2; size_t delim2_idx;
    size_t len;

    delim1 = strchr(var1, '=');
    delim2 = strchr(var2, '=');

    if (delim1 == NULL || delim2 == NULL)
        return false;

    delim1_idx = delim1 - var1;
    delim2_idx = delim2 - var2;
    len = (delim1_idx <= delim2_idx) ? delim1_idx : delim2_idx;

    return strncmp(var1, var2, len) == 0;
}

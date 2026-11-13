#ifndef _NASH_ENGINE_CONTAINER_CONFIG_ENV_H
#define _NASH_ENGINE_CONTAINER_CONFIG_ENV_H

#include <stdbool.h>

bool nsh_env_is_valid(const char *var);
bool nsh_env_cmp_key(const char *var1, const char *var2);

#endif

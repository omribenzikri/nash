#include "engine/common/container_config.h"
#include "engine/image/oci/common.h"
#include "engine/image/oci/config.h"

void nsh_oci_config_load(nsh_oci_image_meta_t *meta, nsh_container_config_t *config)
{
    config->user = meta->config->config->user;
    config->working_dir = meta->config->config->working_dir;
    config->env = meta->config->config->env;
    config->env_len = meta->config->config->env_len;
    config->entrypoint = meta->config->config->entrypoint;
    config->entrypoint_len = meta->config->config->entrypoint_len;
    config->cmd = meta->config->config->cmd;
    config->cmd_len = meta->config->config->cmd_len;
}

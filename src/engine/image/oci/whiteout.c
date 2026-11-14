#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <linux/limits.h>
#include "common/utils/filesystem.h"
#include "engine/image/oci/whiteout.h"

#define OCI_WHITEOUT_PREFIX ".wh."
#define OCI_OPAQUE_WHITEOUT_NAME ".wh..wh..opq"

bool nsh_oci_is_whiteout_new(const char *path)
{
    char base_name[PATH_MAX];
    bool has_prefix;

    split_path(path, NULL, 0, base_name, sizeof(base_name));

    has_prefix = strncmp(base_name, OCI_WHITEOUT_PREFIX,
        strlen(OCI_WHITEOUT_PREFIX)) == 0;

    return has_prefix && !nsh_oci_is_opaque_whiteout_new(path);
}

bool nsh_oci_is_opaque_whiteout_new(const char *path)
{
    char base_name[PATH_MAX];

    split_path(path, NULL, 0, base_name, sizeof(base_name));

    return strcmp(base_name, OCI_OPAQUE_WHITEOUT_NAME) == 0;
}

bool nsh_oci_is_valid_whiteout_new(const char *path)
{
    char base_name[PATH_MAX];

    split_path(path, NULL, 0, base_name, sizeof(base_name));

    return strcmp(base_name, OCI_WHITEOUT_PREFIX) != 0;
}

int nsh_oci_whiteout_to_file_new(const char *path, char *buf, size_t size)
{
    char dir_name[PATH_MAX];
    char base_name[PATH_MAX];

    split_path(path,
        dir_name, sizeof(dir_name),
        base_name, sizeof(base_name));

    if (strlen(base_name) <= strlen(OCI_WHITEOUT_PREFIX))
        return -1;

    snprintf(buf, size, "%s/%s", dir_name, base_name + strlen(OCI_WHITEOUT_PREFIX));

    return 0;
}

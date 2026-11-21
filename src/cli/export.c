#include <stdio.h>
#include <getopt.h>
#include <engine/image/lifecycle.h>
#include "cli/export.h"

static int nsh_cli_export_parse_args(int argc, char **argv, char **name, char **path)
{
    if (argc < 3) {
        fprintf(stderr,
            "Usage: nash %s NAME PATH\n",
            NSH_CLI_EXPORT_CMD);
        return -1;
    }

    *name = argv[1];
    *path = argv[2];

    return 0;
}

int nsh_cli_export_handle(int argc, char **argv)
{
    char *name;
    char *path;
    int err;

    argv += optind;
    argc -= optind;

    err = nsh_cli_export_parse_args(argc, argv, &name,  &path);
    if (err)
        return -1;

    err = nsh_image_export(path, name);
    if (err)
        return -1;

    return 0;
}

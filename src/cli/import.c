#include <stdio.h>
#include <getopt.h>
#include <engine/image/lifecycle.h>
#include "cli/import.h"

static int nsh_cli_import_parse_args(int argc, char **argv, char **path, char **name)
{
    if (argc < 3) {
        fprintf(stderr,
            "Usage: nash %s PATH NAME\n",
            NSH_CLI_IMPORT_CMD);
        return -1;
    }

    *path = argv[1];
    *name = argv[2];

    return 0;
}

int nsh_cli_import_handle(int argc, char **argv)
{
    char *path;
    char *name;
    int err;

    argv += optind;
    argc -= optind;

    err = nsh_cli_import_parse_args(argc, argv, &path, &name);
    if (err)
        return -1;

    err = nsh_image_import(path, name);
    if (err)
        return -1;

    return 0;
}

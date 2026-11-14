#include <stdio.h>
#include <getopt.h>
#include <engine/image/lifecycle.h>
#include "cli/import.h"

static int nsh_cli_import_parse_args(int argc, char **argv, char **path)
{
    if (argc < 2) {
        fprintf(stderr,
            "Usage: nash %s PATH\n",
            NSH_CLI_IMPORT_CMD);
        return -1;
    }

    *path = argv[1];

    return 0;
}

int nsh_cli_import_handle(int argc, char **argv)
{
    char *path;
    int err;

    argv += optind;
    argc -= optind;

    err = nsh_cli_import_parse_args(argc, argv, &path);
    if (err)
        return -1;

    err = nsh_image_import(path);
    if (err)
        return -1;

    return 0;
}

#include <stdio.h>
#include <getopt.h>
#include <engine/image/lifecycle.h>
#include "cli/rmi.h"

static int nsh_cli_rmi_parse_args(int argc, char **argv, char **name)
{
    if (argc < 2) {
        fprintf(stderr,"Usage: nash %s NAME\n", NSH_CLI_RMI_CMD);
        return -1;
    }
    *name = argv[1];

    return 0;
}

int nsh_cli_rmi_handle(int argc, char **argv)
{
    char *name;
    int err;

    argv += optind;
    argc -= optind;

    err = nsh_cli_rmi_parse_args(argc, argv, &name);
    if (err)
        return -1;

    err = nsh_image_delete(name);
    if (err)
        return -1;

    return 0;
}

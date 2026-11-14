#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <cli/run.h>
#include "startup.h"

static int nsh_handle_command(int argc, char **argv)
{
    char *cmd;

    if (argc < 2) {
        fprintf(stderr, "Usage: nash COMMAND\n");
        return -1;
    }
    cmd = argv[1];

    if (strncmp(cmd, NSH_CLI_RUN_CMD, ARG_MAX) == 0) {
        return nsh_cli_run_handle(argc, argv);
    }
    else {
        fprintf(stderr, "Error: unknown command nash %s\n", cmd);
        return -1;
    }
}

int main(int argc, char **argv)
{
    bool system_compat;
    int err;

    err = nsh_verify_system_comapt(&system_compat);
    if (err || !system_compat) {
        fprintf(stderr, "[x] Failed to verify system compatibility, exiting\n");
        return EXIT_FAILURE;
    }

    err = nsh_startup();
    if (err)
        return EXIT_FAILURE;

    err = nsh_handle_command(argc, argv);
    if (err)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

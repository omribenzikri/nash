#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <locale.h>
#include <sys/statfs.h>
#include <linux/magic.h>
#include <linux/limits.h>
#include <engine/startup.h>
#include <cli/run.h>
#include <cli/import.h>
#include <cli/rmi.h>
#include "cli/lsi.h"

static int nsh_is_compat_cgroup_version(bool *out)
{
    struct statfs st;
    int err;

    err = statfs("/sys/fs/cgroup", &st);
    if (err) {
        perror("[x] Failed to fetch cgroup filesystem type");
        return -1;
    }

    *out = st.f_type == CGROUP2_SUPER_MAGIC;

    return 0;
}

static int nsh_verify_system_comapt(bool *out)
{
    int err;

    err = nsh_is_compat_cgroup_version(out);
    if (err)
        return -1;
    if (!*out) {
        fprintf(stderr, "[x] Unsupported cgroup version detected\n");
        return 0;
    }

    return 0;
}

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
    else if (strncmp(cmd, NSH_CLI_IMPORT_CMD, ARG_MAX) == 0) {
        return nsh_cli_import_handle(argc, argv);
    }
    else if (strncmp(cmd, NSH_CLI_RMI_CMD, ARG_MAX) == 0) {
        return nsh_cli_rmi_handle(argc, argv);
    }
    else if (strncmp(cmd, NSH_CLI_LSI_CMD, ARG_MAX) == 0) {
        return nsh_cli_lsi_handle();
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

    setlocale(LC_ALL, "");

    err = nsh_verify_system_comapt(&system_compat);
    if (err || !system_compat) {
        fprintf(stderr, "[x] Failed to verify system compatibility, exiting\n");
        return EXIT_FAILURE;
    }

    err = nsh_engine_startup();
    if (err)
        return EXIT_FAILURE;

    err = nsh_handle_command(argc, argv);
    if (err)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

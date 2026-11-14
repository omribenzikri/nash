#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <sys/wait.h>
#include "common/utils/string.h"
#include "engine/container/init.h"
#include "engine/container/container.h"
#include "daemon/container.h"
#include "cli/run.h"

#define NSH_OPT_RUN_ENV_MAX 1024

enum {
    NSH_OPT_RUN_ENV,
    NSH_OPT_RUN_USER,
    NSH_OPT_RUN_HOSTNAME,
    NSH_OPT_RUN_ENTRYPOINT,
    NSH_OPT_RUN_WORKDIR,
    NSH_OPT_RUN_CPU_TIME,
    NSH_OPT_RUN_PROCS,
    NSH_OPT_RUN_MEMORY,
    NSH_OPT_RUN_MEMORY_SWAP
};

static const struct option nsh_cli_run_opts[] = {
    { "hostname",    required_argument, NULL, NSH_OPT_RUN_HOSTNAME },
    { "env",         required_argument, NULL, NSH_OPT_RUN_ENV },
    { "user",        required_argument, NULL, NSH_OPT_RUN_USER },
    { "entrypoint",  required_argument, NULL, NSH_OPT_RUN_ENTRYPOINT },
    { "workdir",     required_argument, NULL, NSH_OPT_RUN_WORKDIR },
    { "cpu-time",    required_argument, NULL, NSH_OPT_RUN_CPU_TIME },
    { "procs",       required_argument, NULL, NSH_OPT_RUN_PROCS },
    { "memory",      required_argument, NULL, NSH_OPT_RUN_MEMORY },
    { "memory-swap", required_argument, NULL, NSH_OPT_RUN_MEMORY_SWAP },
    { NULL,          0,                 NULL, 0 }
};

static int nsh_cli_run_parse_opts(int argc, char **argv,
    nsh_container_params_t *params)
{
    int opt;
    int err;

    static char *env[NSH_OPT_RUN_ENV_MAX] = {0};
    static size_t env_len = 0;
    static char *entrypoint = NULL;

    while ((opt = getopt_long(argc, argv, "", nsh_cli_run_opts, NULL)) != -1) {
        switch (opt) {
        case NSH_OPT_RUN_HOSTNAME:
            if (strlen(optarg) > NSH_OPT_RUN_ENV_MAX) {
                fprintf(stderr, "Error: hostname is too long\n");
                return -1;
            }
            params->hostname = optarg;
            break;
        case NSH_OPT_RUN_ENV:
            if (env_len >= NSH_OPT_RUN_ENV_MAX) {
                fprintf(stderr,
                    "Error: too many environment variables provided\n");
                return -1;
            }
            env[env_len] = optarg;
            env_len++;
            break;
        case NSH_OPT_RUN_USER:
            params->config.user = optarg;
            break;
        case NSH_OPT_RUN_ENTRYPOINT:
            if (strlen(optarg) > PATH_MAX) {
                fprintf(stderr, "Error: entrypoint is too long\n");
                return -1;
            }
            entrypoint = optarg;
            break;
        case NSH_OPT_RUN_WORKDIR:
            if (strlen(optarg) > PATH_MAX) {
                fprintf(stderr, "Error: workdir is too long\n");
                return -1;
            }
            params->config.working_dir = optarg;
            break;
        case NSH_OPT_RUN_CPU_TIME:
            err = str_parse_uintmax(optarg, 10, UINT8_MAX,
                (uintmax_t *)&params->resources.cpu_time);
            if (err ||params->resources.cpu_time == 0
                    ||params->resources.cpu_time > 100) {
                        fprintf(stderr, "Error: cpu-time is invalid\n");
                        return -1;
                    }
            break;
        case NSH_OPT_RUN_PROCS:
            err = str_parse_uintmax(optarg, 10, UINT32_MAX,
                (uintmax_t *)&params->resources.procs);
            if (err) {
                fprintf(stderr, "Error: procs is invalid\n");
                return -1;
            }
            break;
        case NSH_OPT_RUN_MEMORY:
            err = str_parse_uintmax(optarg, 10, UINT64_MAX,
                (uintmax_t *)&params->resources.memory_bytes);
            if (err) {
                fprintf(stderr, "Error: memory is invalid\n");
                return -1;
            }
            break;
        case NSH_OPT_RUN_MEMORY_SWAP:
            err = str_parse_uintmax(optarg, 10, UINT64_MAX,
                (uintmax_t *)&params->resources.memory_swap_bytes);
            if (err) {
                fprintf(stderr, "Error: swap is invalid\n");
                return -1;
            }
            break;
        default:
            return -1;
        }
    }

    params->config.entrypoint = &entrypoint;
    params->config.entrypoint_len = entrypoint != NULL ? 1 : 0;
    params->config.env = env;
    params->config.env_len = env_len;

    return 0;
}

static int nsh_cli_run_parse_args(int argc, char **argv,
    nsh_container_params_t *params)
{
    if (argc < 2) {
        fprintf(stderr,
            "Usage: nash %s [OPTIONS] IMAGE [COMMAND] [ARG...]\n",
            NSH_CLI_RUN_CMD);
        return -1;
    }

    params->image_path = argv[1];
    params->config.cmd = &argv[2];
    params->config.cmd_len = argc - 2;

    return 0;
}

int nsh_cli_run_handle(int argc, char **argv)
{
    nsh_container_params_t params = {0};
    nsh_container_t *container;
    int err;
    int ret = 0;

    err = nsh_cli_run_parse_opts(argc, argv, &params);
    if (err) {
        ret = -1;
        goto out;
    }

    argv += optind;
    argc -= optind;

    err = nsh_cli_run_parse_args(argc, argv, &params);
    if (err) {
        ret = -1;
        goto out;
    }

    err = nsh_container_create(&params, &container);
    if (err) {
        fprintf(stderr, "[x] Failed to create container, exiting\n");
        ret = -1;
        goto out;
    }

    if (waitpid(container->pid, NULL, 0) == -1) {
        perror("[x] Awaiting child failed");
        ret = -1;
        goto out_container_delete;
    }

out_container_delete:
    nsh_container_delete(container);
out:
    return ret;
}

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/wait.h>
#include "resources/container.h"
#include "startup.h"

#define NSH_OPT_ENV_MAX 1024

enum {
    NSH_OPT_ENV,
    NSH_OPT_HOSTNAME,
    NSH_OPT_ENTRYPOINT,
    NSH_OPT_WORKDIR,
    NSH_OPT_CPU_TIME,
    NSH_OPT_PROCS,
    NSH_OPT_MEMORY,
    NSH_OPT_MEMORY_SWAP
};

static const struct option nsh_options[] = {
    { "hostname",    required_argument, NULL, NSH_OPT_HOSTNAME },
    { "env",         required_argument, NULL, NSH_OPT_ENV },
    { "entrypoint",  required_argument, NULL, NSH_OPT_ENTRYPOINT },
    { "workdir",     required_argument, NULL, NSH_OPT_WORKDIR },
    { "cpu-time",    required_argument, NULL, NSH_OPT_CPU_TIME },
    { "procs",       required_argument, NULL, NSH_OPT_PROCS },
    { "memory",      required_argument, NULL, NSH_OPT_MEMORY },
    { "memory-swap", required_argument, NULL, NSH_OPT_MEMORY_SWAP },
    { NULL,          0,                 NULL,  0  }
};

static int nsh_parse_uint(const char *str, uintmax_t max, uintmax_t *result)
{
    char *end;
    uintmax_t value;

    if (str == NULL || *str == '\0' || *str == '-')
        return -1;

    errno = 0;
    value = strtoumax(str, &end, 10);

    if (errno == ERANGE || end == str || *end != '\0' || value > max)
        return -1;

    *result = value;
    return 0;
}

static int nsh_parse_options(int argc, char **argv, nsh_container_params_t *params)
{
    int opt;
    int err;

    static char *env[NSH_OPT_ENV_MAX] = {0};
    static size_t env_len = 0;
    static char *entrypoint = NULL;

    while ((opt = getopt_long(argc, argv, "", nsh_options, NULL)) != -1) {
        switch (opt) {
        case NSH_OPT_HOSTNAME:
            if (strlen(optarg) > HOST_NAME_MAX)
                return -1;

            params->hostname = optarg;
            break;
        case NSH_OPT_ENV:
            if (env_len >= NSH_OPT_ENV_MAX)
                return -1;

            env[env_len] = optarg;
            env_len++;
            break;
        case NSH_OPT_ENTRYPOINT:
            if (strlen(optarg) > PATH_MAX)
                return -1;

            entrypoint = optarg;
            break;
        case NSH_OPT_WORKDIR:
            if (strlen(optarg) > PATH_MAX)
                return -1;

            params->config.working_dir = optarg;
            break;
        case NSH_OPT_CPU_TIME:
            err = nsh_parse_uint(optarg, UINT8_MAX,
                                (uintmax_t *)&params->resources.cpu_time);
            if (err ||params->resources.cpu_time == 0
                    || params->resources.cpu_time > 100)
                return -1;
            break;
        case NSH_OPT_PROCS:
            err = nsh_parse_uint(optarg, UINT32_MAX,
                                (uintmax_t *)&params->resources.procs);
            if (err)
                return -1;
            break;
        case NSH_OPT_MEMORY:
            err = nsh_parse_uint(optarg, UINT64_MAX,
                                (uintmax_t *)&params->resources.memory_bytes);
            if (err)
                return -1;
            break;
        case NSH_OPT_MEMORY_SWAP:
            err = nsh_parse_uint(optarg, UINT64_MAX,
                                (uintmax_t *)&params->resources.memory_swap_bytes);
            if (err)
                return -1;
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

static void nsh_parse_args(int argc, char **argv, nsh_container_params_t *params)
{
    params->image_path = argv[0];
    params->config.cmd = &argv[1];
    params->config.cmd_len = argc - 1;
}

int main(int argc, char **argv)
{
    nsh_container_params_t params = {0};
    nsh_container_t *container;
    bool system_compat;
    int err;

    err = nsh_parse_options(argc, argv, &params);
    if (err) {
        fprintf(stderr, "Error: invalid arguments supplied\n");
        return EXIT_FAILURE;
    }

    if (argc - optind < 1) {
        fprintf(stderr, "Usage: nash [OPTIONS] IMAGE [COMMAND] [ARG...]\n");
        return EXIT_FAILURE;
    }

    nsh_parse_args(argc - optind, argv + optind, &params);

    err = nsh_verify_system_comapt(&system_compat);
    if (err || !system_compat) {
        fprintf(stderr, "[x] Failed to verify system compatibility, exiting\n");
        return EXIT_FAILURE;
    }

    err = nsh_startup();
    if (err)
        return EXIT_FAILURE;

    err = nsh_container_create(&params, &container);
    if (err) {
        fprintf(stderr, "[x] Failed to create container, exiting\n");
        return EXIT_FAILURE;
    }

    if (waitpid(container->pid, NULL, 0) == -1) {
        perror("[x] Awaiting child failed");
        return EXIT_FAILURE;
    }

    nsh_container_delete(container);

    return EXIT_SUCCESS;
}

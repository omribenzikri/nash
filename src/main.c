#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/wait.h>
#include "container.h"
#include "startup.h"

static const struct option nsh_options[] = {
    { "cpu-time",    required_argument, NULL, 'c' },
    { "procs",       required_argument, NULL, 'p' },
    { "memory",      required_argument, NULL, 'm' },
    { "memory-swap", required_argument, NULL, 's' },
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

static int nsh_parse_options(int argc, char **argv, nsh_container_config_t *config)
{
    int opt;
    int err;

    while ((opt = getopt_long(argc, argv, "", nsh_options, NULL)) != -1) {
        switch (opt) {
        case 'c':
        err = nsh_parse_uint(optarg, UINT8_MAX,
                            (uintmax_t *)&config->resources.cpu_time);
        if (err ||config->resources.cpu_time == 0
                || config->resources.cpu_time > 100)
            return -1;
        break;
        case 'p':
            err = nsh_parse_uint(optarg, UINT32_MAX,
                                (uintmax_t *)&config->resources.procs);
            if (err)
                return -1;
            break;
        case 'm':
            err = nsh_parse_uint(optarg, UINT64_MAX,
                                (uintmax_t *)&config->resources.memory_bytes);
            if (err)
                return -1;
            break;
        case 's':
            err = nsh_parse_uint(optarg, UINT64_MAX,
                                (uintmax_t *)&config->resources.memory_swap_bytes);
            if (err)
                return -1;
            break;
        default:
            return -1;
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    nsh_container_config_t config = {0};
    nsh_container_t container = {0};
    char *envp[] = {NULL};
    bool system_compat;
    int err;

    err = nsh_parse_options(argc, argv, &config);
    if (err) {
        fprintf(stderr, "Error: invalid arguments supplied\n");
        return EXIT_FAILURE;
    }

    if (argc - optind < 2) {
        fprintf(stderr, "Usage: nash [OPTIONS] IMAGE COMMAND [ARG...]\n");
        return EXIT_FAILURE;
    }

    config.image_path = argv[optind];
    config.exec_path = argv[optind + 1];
    config.exec_argv = argv + optind + 1;
    config.exec_envp = envp;

    err = nsh_verify_system_comapt(&system_compat);
    if (err || !system_compat) {
        fprintf(stderr, "[x] Failed to verify system compatibility, exiting\n");
        return EXIT_FAILURE;
    }

    err = nsh_startup();
    if (err)
        return EXIT_FAILURE;

    err = nsh_container_create(&config, &container);
    if (err) {
        fprintf(stderr, "[x] Failed to create container, exiting\n");
        return EXIT_FAILURE;
    }

    if (waitpid(container.pid, NULL, 0) == -1) {
        perror("[x] Awaiting child failed");
        return EXIT_FAILURE;
    }

    nsh_container_delete(&container);

    return EXIT_SUCCESS;
}

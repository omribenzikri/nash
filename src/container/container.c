#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/sched.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include "utils/file.h"
#include "container/common.h"
#include "container/state.h"
#include "container/filesystem.h"
#include "container/capabilities.h"
#include "container/creds.h"
#include "container/resources.h"
#include "container/container.h"

#define CNTR_GEN_HOSTNAME_LEN 13

static char *cntr_root_path = NULL;

static int cntr_proc_starttime_get(pid_t pid, uint64_t *out)
{
    char path[PATH_MAX];
    FILE *stream;
    char buffer[4096];
    size_t nread;
    char *cursor;
    char *end;
    int ret = 0;

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    stream = fopen(path, "r");
    if (stream == NULL) {
        perror("[x] Failed to open process stat file");
        ret = -1;
        goto out;
    }

    nread = fread(buffer, 1, sizeof(buffer) - 1, stream);
    if (nread == 0) {
        perror("[x] Failed to read from process stat file");
        ret = -1;
        goto out_close_stream;
    }
    buffer[nread] = '\0';

    // Skip the comm field (which can contain spaces)
    cursor = strrchr(buffer, ')');
    if (cursor == NULL) {
        fprintf(stderr, "[x] Failed to parse process stat file\n");
        ret = -1;
        goto out_close_stream;
    }
    cursor += 3;

    // Skip fields 3..21
    for (int i = 0; i < 19; i++) {
        cursor = strchr(cursor, ' ');
        if (cursor == NULL) {
            fprintf(stderr, "[x] Failed to parse process stat file\n");
            ret = -1;
            goto out_close_stream;
        }
        cursor++;
    }

    *out = strtoull(cursor, &end, 10);
    if (end == cursor) {
        fprintf(stderr, "[x] Failed to parse process start time\n");
        ret = -1;
        goto out_close_stream;
    }

out_close_stream:
    fclose(stream);
out:
    return ret;
}

static int cntr_proc_btime_get(uint64_t *out)
{
    FILE *stream;
    char buffer[4096];
    uint64_t btime;
    int matched;
    int ret = 0;

    stream = fopen("/proc/stat", "r");
    if (stream == NULL) {
        perror("[x] Failed to open stat file");
        ret = -1;
        goto out;
    }

    while (fgets(buffer, sizeof(buffer), stream) != NULL) {
        matched = sscanf(buffer, "btime %lu", &btime);
        if (matched == 1) {
            *out = btime;
            goto out_close_stream;
        }
    }

    fprintf(stderr, "[x] Failed to read boot time\n");
    ret = -1;

out_close_stream:
    fclose(stream);
out:
    return ret;
}

static int cntr_start_time_resolve(pid_t pid, uint64_t *out)
{
    uint64_t ticks;
    uint64_t btime;
    long hz;
    int err;

    err = cntr_proc_starttime_get(pid, &ticks);
    if (err)
        return -1;

    err = cntr_proc_btime_get(&btime);
    if (err)
        return -1;

    hz = sysconf(_SC_CLK_TCK);
    *out = (btime + (ticks / hz));

    return 0;
}

static int cntr_workspace_path_resolve(uuid_t id, char *buf, size_t size)
{
    char uuid_string[UUID_STR_LEN];

    if (cntr_root_path == NULL) {
        fprintf(stderr, "[x] Failed to resolve workspace path, root not set\n");
        return -1;
    }

    uuid_unparse_lower(id, uuid_string);
    snprintf(buf, size, "%s/%s", cntr_root_path, uuid_string);

    return 0;
}

static void cntr_hostname_generate(uuid_t id, char *buf, size_t size)
{
    char uuid_string[UUID_STR_LEN];
    char *hostname;

    assert(CNTR_GEN_HOSTNAME_LEN < HOST_NAME_MAX);
    assert(CNTR_GEN_HOSTNAME_LEN < UUID_STR_LEN);
    uuid_unparse_lower(id, uuid_string);

    hostname = uuid_string + sizeof(uuid_string) - CNTR_GEN_HOSTNAME_LEN;
    snprintf(buf, size, "%s", hostname);
}

static int cntr_exec(cntr_config_t *config)
{
    char *path;
    char **argv = NULL;
    char **envp = NULL;

    if (config->exec_len < 1) {
        fprintf(stderr, "[x] Failed to execute: no executable provided\n");
        goto error;
    }

    path = config->exec[0];

    argv = realloc(config->exec, (config->exec_len + 1) * sizeof(*argv));
    if (argv == NULL) {
        perror("[x] Failed to allocate argv");
        goto error;
    }
    argv[config->exec_len] = NULL;

    envp = realloc(config->env, (config->env_len + 1) * sizeof(*envp));
    if (envp == NULL) {
        perror("[x] Failed to allocate envp");
        goto error;
    }
    envp[config->env_len] = NULL;

    execve(path, argv, envp);
    perror("[x] Failed to execute container program");

error:
    if (argv) free(argv);
    if (envp) free(envp);
    return -1;
}

static int cntr_init(cntr_config_t *config)
{
    int err;

    err = sethostname(config->hostname, strlen(config->hostname));
    if (err) {
        perror("[x] Failed to set container hostname");
        return -1;
    }

    err = cntr_pivot_root(config->rootfs_path, config->workdir) ||
          cntr_filesystem_init() ||
          cntr_caps_drop() ||
          cntr_creds_set(&config->creds);
    if (err)
        exit(EXIT_FAILURE);

    cntr_exec(config);
    exit(EXIT_FAILURE);
}

static pid_t cntr_clone(cntr_config_t *config, int cgroup_fd)
{
    const int namespace_flags =
        CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID |
        CLONE_NEWIPC | CLONE_NEWCGROUP | CLONE_NEWTIME;
    struct clone_args args = {
        .flags  = namespace_flags | CLONE_INTO_CGROUP,
        .exit_signal = SIGCHLD,
        .cgroup = cgroup_fd
    };
    pid_t pid;

    pid = syscall(SYS_clone3, &args, sizeof(args));
    if (pid == -1) {
        perror("[x] Container clone failed");
        return -1;
    }

    if (pid == 0) {
        cntr_init(config);
        assert(1);
    }

    return pid;
}

static pid_t cntr_create_internal(uuid_t id, cntr_config_t *config)
{
    int cgroup_fd;
    pid_t pid;

    cgroup_fd = cntr_cgroup_open(id);
    if (cgroup_fd == -1)
        return -1;

    pid = cntr_clone(config, cgroup_fd);
    close(cgroup_fd);

    return pid;
}

int cntr_create(uuid_t id, cntr_config_t *config, pid_t *out)
{
    char hostname[HOST_NAME_MAX];
    char workspace_path[PATH_MAX];
    cntr_state_t state = {0};
    int err;

    if (config->hostname == NULL) {
        cntr_hostname_generate(id, hostname, sizeof(hostname));
        config->hostname = hostname;
    }

    cntr_workspace_path_resolve(id, workspace_path, sizeof(workspace_path));

    err = mkdir(workspace_path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create container workspace");
        goto error;
    }

    err = cntr_cgroup_create(id, &config->resources);
    if (err)
        goto error_del_workspace;

    uuid_copy(state.id, id);
    state.config = *config;

    state.pid = cntr_create_internal(id, config);
    if (state.pid == -1)
        goto error_del_cgroup;

    err = cntr_start_time_resolve(state.pid, &state.start_time);
    if (err)
        // TODO: cleanup created process
        goto error_del_cgroup;

    err = cntr_state_dump(&state, workspace_path);
    if (err)
        // TODO: cleanup created process
        goto error_del_cgroup;

    *out = state.pid;
    return 0;

error_del_cgroup:
    cntr_cgroup_delete(id);
error_del_workspace:
    err = rmdir_recursive(workspace_path);
    if (err)
        perror("[x] Failed to delete container workspace");
error:
    return -1;
}

int cntr_delete(uuid_t id)
{
    char workspace_path[PATH_MAX];
    int err;

    cntr_workspace_path_resolve(id, workspace_path, sizeof(workspace_path));

    err = cntr_cgroup_delete(id);
    if (err)
        return -1;

    err = rmdir_recursive(workspace_path);
    if (err) {
        perror("[x] Failed to delete container workspace");
        return -1;
    }

    return 0;
}

int cntr_settings_set_root(const char *path)
{
    char *copy;

    if (path == NULL) {
        fprintf(stderr, "[x] Failed to set root, null provided\n");
        return -1;
    }

    copy = strdup(path);
    if (copy == NULL) {
        perror("[x] Failed to allocate root path");
        return -1;
    }

    if (cntr_root_path != NULL)
        free(cntr_root_path);
    cntr_root_path = copy;

    return 0;
}

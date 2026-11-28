#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
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
#include "container/filesystem.h"
#include "container/capabilities.h"
#include "container/creds.h"
#include "container/resources.h"
#include "container/container.h"

#define CNTR_GEN_HOSTNAME_LEN 13

static char *cntr_root_path = NULL;

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

static int cntr_hostname_set(uuid_t id, const char *hostname)
{
    char buf[HOST_NAME_MAX];
    int err;

    if (hostname == NULL)
        cntr_hostname_generate(id, buf, sizeof(buf));
    else
        snprintf(buf, sizeof(buf), "%s", hostname);

    err = sethostname(buf, strlen(buf));
    if (err) {
        perror("[x] Failed to set container hostname");
        return -1;
    }

    return 0;
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

static int cntr_init(uuid_t id, cntr_config_t *config)
{
    int err;

    err = cntr_hostname_set(id, config->hostname) ||
          cntr_pivot_root(config->rootfs_path, config->workdir) ||
          cntr_filesystem_init() ||
          cntr_caps_drop() ||
          cntr_creds_set(&config->creds);
    if (err)
        exit(EXIT_FAILURE);

    cntr_exec(config);
    exit(EXIT_FAILURE);
}

static pid_t cntr_clone(
    uuid_t id, cntr_config_t *config, int cgroup_fd)
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
        cntr_init(id, config);
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

    pid = cntr_clone(id, config, cgroup_fd);
    close(cgroup_fd);

    return pid;
}

int cntr_create(uuid_t id, cntr_config_t *config, pid_t *out)
{
    char workspace_path[PATH_MAX];
    pid_t pid;
    int err;

    cntr_workspace_path_resolve(id, workspace_path, sizeof(workspace_path));

    err = mkdir(workspace_path, S_IRWXU);
    if (err) {
        perror("[x] Failed to create container workspace");
        goto error;
    }

    err = cntr_cgroup_create(id, &config->resources);
    if (err)
        goto error_del_workspace;

    pid = cntr_create_internal(id, config);
    if (pid == -1)
        goto error_del_cgroup;

    *out = pid;
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

#include <stdio.h>
#include <stdbool.h>
#include <sys/statfs.h>
#include <linux/magic.h>
#include "startup.h"

static int nsh_verify_cgroup_version(bool *out)
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

int nsh_verify_system_comapt(bool *out)
{
    int err;

    err = nsh_verify_cgroup_version(out);
    if (err)
        return -1;
    if (!*out) {
        fprintf(stderr, "[x] Unsupported cgroup version detected\n");
        return 0;
    }

    return 0;
}

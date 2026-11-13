#include <stdio.h>
#include <stdbool.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <linux/capability.h>
#include <linux/prctl.h>
#include "utils/macros.h"
#include "runtime/capabilities.h"

static const cap_value_t nsh_allowed_caps[] = {
    CAP_DAC_OVERRIDE,
    CAP_CHOWN,
    CAP_FOWNER,
    CAP_FSETID,
    CAP_SETGID,
    CAP_SETUID,
    CAP_SETFCAP,
    CAP_SETPCAP,
    CAP_NET_BIND_SERVICE,
    CAP_NET_RAW,
    CAP_SYS_CHROOT,
    CAP_KILL,
};

static int nsh_cap_is_allowed(cap_value_t cap)
{
    size_t allowed_caps_len = ARRAY_LEN(nsh_allowed_caps);

    for (size_t i = 0; i < allowed_caps_len; i++) {
        if (nsh_allowed_caps[i] == cap)
            return true;
    }

    return false;
}

static int nsh_caps_set_populate(cap_t caps, cap_flag_t set)
{
    size_t allowed_caps_len = ARRAY_LEN(nsh_allowed_caps);
    int err;

    err = cap_set_flag(caps, set,
        allowed_caps_len, nsh_allowed_caps, CAP_SET);
    if (err) {
        perror("[x] Failed to set capabilities whitelist");
        return -1;
    }

    return 0;
}

static int nsh_caps_standard_sets_restrict()
{
    cap_t caps;
    int ret = 0;
    int err;

    caps = cap_init();
    if (caps == NULL) {
        perror("[x] Failed to allocate capability state");
        ret = -1;
        goto out;
    }

    err = nsh_caps_set_populate(caps, CAP_EFFECTIVE) ||
          nsh_caps_set_populate(caps, CAP_PERMITTED) ||
          nsh_caps_set_populate(caps, CAP_INHERITABLE);
    if (err) {
        ret = -1;
        goto out_free_caps;
    }

    err = cap_set_proc(caps);
    if (err) {
        perror("[x] Failed to apply capability state to process");
        ret = -1;
        goto out_free_caps;
    }

out_free_caps:
    err = cap_free(caps);
    if (err) {
        perror("[x] Failed to free capability state");
        ret = -1;
    }
out:
    return ret;
}

static int nsh_caps_bounding_set_restrict()
{
    int err;

    for (cap_value_t cap = 0; cap <= CAP_LAST_CAP; cap++) {
        if (nsh_cap_is_allowed(cap))
            continue;

        err = prctl(PR_CAPBSET_DROP, cap);
        if (err) {
            perror("[x] Failed to drop capability from bounding set");
            return -1;
        }
    }

    return 0;
}

static int nsh_caps_ambient_set_clear()
{
    int err;

    err = prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0);
    if (err) {
        perror("[x] Failed to clear ambient capabilities set");
        return -1;
    }

    return 0;
}

static int nsh_set_no_new_privs()
{
    int err;

    err = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    if (err) {
        perror("[x] Failed to set process no_new_privs attribute");
        return -1;
    }

    return 0;
}

int nsh_caps_drop()
{
    return nsh_set_no_new_privs()           ||
           nsh_caps_bounding_set_restrict() ||
           nsh_caps_ambient_set_clear()     ||
           nsh_caps_standard_sets_restrict();
}

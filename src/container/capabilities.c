#include <stdio.h>
#include <stdbool.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <linux/capability.h>
#include <linux/prctl.h>
#include "utils/macros.h"
#include "container/capabilities.h"

static const cap_value_t cntr_caps[] = {
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

static int cntr_set_no_new_privs()
{
    int err;

    err = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    if (err) {
        perror("[x] Failed to set process no_new_privs attribute");
        return -1;
    }

    return 0;
}

static int cntr_caps_is_allowed(cap_value_t cap)
{
    for (size_t i = 0; i < ARRAY_LEN(cntr_caps); i++) {
        if (cntr_caps[i] == cap)
            return true;
    }

    return false;
}

static int cntr_caps_set_populate(cap_t caps, cap_flag_t set)
{
    int err;

    err = cap_set_flag(caps, set, ARRAY_LEN(cntr_caps), cntr_caps, CAP_SET);
    if (err) {
        perror("[x] Failed to populate capabilities set");
        return -1;
    }

    return 0;
}

static int cntr_caps_standard_sets_filter()
{
    cap_t caps;
    int err;
    int ret = 0;

    caps = cap_init();
    if (caps == NULL) {
        perror("[x] Failed to allocate capability state");
        ret = -1;
        goto out;
    }

    err = cntr_caps_set_populate(caps, CAP_EFFECTIVE) ||
          cntr_caps_set_populate(caps, CAP_PERMITTED) ||
          cntr_caps_set_populate(caps, CAP_INHERITABLE);
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

static int cntr_caps_bounding_set_filter()
{
    int err;

    for (cap_value_t cap = 0; cap <= CAP_LAST_CAP; cap++) {
        if (cntr_caps_is_allowed(cap))
            continue;

        err = prctl(PR_CAPBSET_DROP, cap);
        if (err) {
            perror("[x] Failed to drop capability from bounding set");
            return -1;
        }
    }

    return 0;
}

static int cntr_caps_ambient_set_clear()
{
    int err;

    err = prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0);
    if (err) {
        perror("[x] Failed to clear ambient capabilities set");
        return -1;
    }

    return 0;
}

int cntr_caps_drop()
{
    int err;

    err = cntr_set_no_new_privs();
    if (err)
        return -1;

    err = cntr_caps_standard_sets_filter() ||
          cntr_caps_bounding_set_filter()  ||
          cntr_caps_ambient_set_clear();

    return err ? -1 : 0;
}

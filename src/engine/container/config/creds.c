#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "utils/array.h"
#include "utils/string.h"
#include "runtime/creds.h"
#include "engine/container/config/creds.h"

#define PASSWD_PATH "/etc/passwd"
#define GROUP_PATH "/etc/group"

int nsh_creds_parse(const char *in, char **out_user, char **out_group)
{
    char buf[LOGIN_NAME_MAX * 2];
    char *user;
    char *group;
    size_t len;

    len = snprintf(buf, sizeof(buf), "%s", in);
    if (len >= sizeof(buf)) {
        fprintf(stderr, "[x] Credentials input is too long\n");
        goto error;
    }

    user = strtok(buf, ":");
    if (user == NULL) {
        fprintf(stderr, "[x] Credentials input is malformed\n");
        goto error;
    }
    group = strtok(NULL, ":");

    *out_user = strndup(user, sizeof(buf));
    if (*out_user == NULL) {
        perror("[x] Failed to allocate user name");
        goto error;
    }

    if (group == NULL)
        *out_group = NULL;
    else {
        *out_group = strndup(group, sizeof(buf));
        if (*out_group == NULL) {
            perror("[x] Failed to allocate group name");
            goto error_free_user;
        }
    }

    return 0;

error_free_user:
    free(*out_user);
error:
    return -1;
}

static FILE *nsh_fopen_passwd(const char *rootfs_path, const char *modes)
{
    char path[PATH_MAX];
    FILE *stream;

    snprintf(path, sizeof(path), "%s%s", rootfs_path, PASSWD_PATH);

    stream = fopen(path, modes);
    if (stream == NULL) {
        perror("[x] Failed to open container " PASSWD_PATH);
        return NULL;
    }

    return stream;
}

static FILE *nsh_fopen_group(const char *rootfs_path, const char *modes)
{
    char path[PATH_MAX];
    FILE *stream;

    snprintf(path, sizeof(path), "%s%s", rootfs_path, GROUP_PATH);

    stream = fopen(path, modes);
    if (stream == NULL) {
        perror("[x] Failed to open container " GROUP_PATH);
        return NULL;
    }

    return stream;
}

static int nsh_uid_lookup(const char *rootfs_path, const char *user, uid_t *out)
{
    struct passwd *pwd;
    FILE *stream;
    int ret = 0;

    stream = nsh_fopen_passwd(rootfs_path, "r");
    if (stream == NULL) {
        ret = -1;
        goto out;
    }

    errno = 0;

    while ((pwd = fgetpwent(stream)) != NULL) {
        if (strncmp(user, pwd->pw_name, LOGIN_NAME_MAX) != 0)
            continue;

        *out = pwd->pw_uid;
        goto out_close_stream;
    }
    if (errno) {
        perror("[x] Failed to read from container " PASSWD_PATH);
        ret = -1;
        goto out_close_stream;
    }

    fprintf(stderr, "[x] User: %s does not exist\n", user);
    ret = -1;

out_close_stream:
    fclose(stream);
out:
    return ret;
}

static int nsh_uname_lookup(const char *rootfs_path, uid_t uid, char **out)
{
    struct passwd *pwd;
    FILE *stream;
    int ret = 0;

    stream = nsh_fopen_passwd(rootfs_path, "r");
    if (stream == NULL) {
        ret = -1;
        goto out;
    }

    errno = 0;

    while ((pwd = fgetpwent(stream)) != NULL) {
        if (uid != pwd->pw_uid)
            continue;

        *out = strndup(pwd->pw_name, LOGIN_NAME_MAX);
        if (*out == NULL) {
            perror("[x] Failed to allocate user name");
            ret = -1;
        }

        goto out_close_stream;
    }
    if (errno) {
        perror("[x] Failed to read from container " PASSWD_PATH);
        ret = -1;
        goto out_close_stream;
    }

    fprintf(stderr, "[x] UID: %u does not exist\n", uid);
    ret = -1;

out_close_stream:
    fclose(stream);
out:
    return ret;
}

static int nsh_gid_lookup(const char *rootfs_path, const char *group, gid_t *out)
{
    struct group *grp;
    FILE *stream;
    int ret = 0;

    stream = nsh_fopen_group(rootfs_path, "r");
    if (stream == NULL) {
        ret = -1;
        goto out;
    }

    errno = 0;

    while ((grp = fgetgrent(stream)) != NULL) {
        if (strncmp(group, grp->gr_name, LOGIN_NAME_MAX) != 0)
            continue;

        *out = grp->gr_gid;
        goto out_close_stream;
    }
    if (errno) {
        perror("[x] Failed to read from container " GROUP_PATH);
        ret = -1;
        goto out_close_stream;
    }

    fprintf(stderr, "[x] Group: %s does not exist\n", group);
    ret = -1;

out_close_stream:
    fclose(stream);
out:
    return ret;
}

static int nsh_default_group_lookup(const char *rootfs_path, uid_t uid, gid_t *out)
{
    struct passwd *pwd;
    FILE *stream;
    int ret = 0;

    stream = nsh_fopen_passwd(rootfs_path, "r");
    if (stream == NULL) {
        ret = -1;
        goto out;
    }

    errno = 0;

    while ((pwd = fgetpwent(stream)) != NULL) {
        if (uid != pwd->pw_uid)
            continue;

        *out = pwd->pw_gid;
        goto out_close_stream;
    }
    if (errno) {
        perror("[x] Failed to read from container " GROUP_PATH);
        ret = -1;
        goto out_close_stream;
    }

    fprintf(stderr, "[x] UID: %u does not exist\n", uid);
    ret = -1;

out_close_stream:
    fclose(stream);
out:
    return ret;
}

static int nsh_supplementary_groups_lookup(
    const char *rootfs_path, uid_t uid, struct cc_array_s *out)
{
    struct group *grp;
    FILE *stream;
    char *uname;

    enum cc_stat cc_stat;
    int ret = 0;
    int err;

    err = nsh_uname_lookup(rootfs_path, uid, &uname);
    if (err) {
        ret = -1;
        goto out;
    }

    stream = nsh_fopen_group(rootfs_path, "r");
    if (stream == NULL) {
        ret = -1;
        goto out_free_uname;
    }

    errno = 0;

    while ((grp = fgetgrent(stream)) != NULL) {
        for (int i = 0; grp->gr_mem[i] != NULL; i++) {
            if (strncmp(uname, grp->gr_mem[i], LOGIN_NAME_MAX) != 0)
                continue;

            cc_stat = cc_array_add_value(out, &grp->gr_gid, sizeof(grp->gr_gid));
            if (cc_stat != CC_OK) {
                fprintf(stderr, "[x] Failed to add supplementary group\n");
                ret = -1;
                goto out_close_stream;
            }
        }
    }
    if (errno) {
        perror("[x] Failed to read from container " GROUP_PATH);
        ret = -1;
        goto out_close_stream;
    }

out_close_stream:
    fclose(stream);
out_free_uname:
    free(uname);
out:
    return ret;
}

static int nsh_uid_resolve(const char *rootfs_path, const char *user, uid_t *out)
{
    int err;

    err = str_parse_uintmax(user, UINT_MAX, (uintmax_t *)out);
    if (!err)
        return 0;

    err = nsh_uid_lookup(rootfs_path, user, out);
    if (err)
        return err;

    return 0;
}

static int nsh_gid_resolve(const char *rootfs_path, const char *group, gid_t *out)
{
    int err;

    err = str_parse_uintmax(group, UINT_MAX, (uintmax_t *)out);
    if (!err)
        return 0;

    err = nsh_gid_lookup(rootfs_path, group, out);
    if (err)
        return err;

    return 0;
}

int nsh_creds_resolve(const char *user, const char *group,
    const char *rootfs_path, nsh_creds_t *out)
{
    struct cc_array_s *sup_gids;
    enum cc_stat cc_stat;
    int err;
    int ret = 0;

    cc_stat = cc_array_new(&sup_gids);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate supplementary groups dynarray\n");
        ret = -1;
        goto out;
    }

    err = nsh_uid_resolve(rootfs_path, user, &out->uid);
    if (err) {
        ret = -1;
        goto out_destroy_sup_gids;
    }

    if (group == NULL) {
        err = nsh_default_group_lookup(rootfs_path, out->uid, &out->gid);
        if (err) {
            ret = -1;
            goto out_destroy_sup_gids;
        }

        err = nsh_supplementary_groups_lookup(rootfs_path, out->uid, sup_gids);
        if (err) {
            ret = -1;
            goto out_destroy_sup_gids;
        }
    }
    else {
        err = nsh_gid_resolve(rootfs_path, group, &out->gid);
        if (err) {
            ret = -1;
            goto out_destroy_sup_gids;
        }
    }

    cc_stat = cc_array_to_static_values(sup_gids, sizeof(gid_t),
        (void **)&out->sup_gids, &out->sup_gids_len);

    if (cc_stat != CC_OK) {
        ret = -1;
        goto out_destroy_sup_gids;
    }

out_destroy_sup_gids:
    cc_array_destroy_cb(sup_gids, free);
out:
    return ret;
}

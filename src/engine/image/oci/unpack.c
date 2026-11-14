#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <archive.h>
#include <archive_entry.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include "common/utils/array.h"
#include "common/utils/filesystem.h"
#include "engine/image/overlay.h"
#include "engine/image/oci/whiteout.h"
#include "engine/image/oci/unpack.h"

#define NSH_OCI_LAYER_UNPACK_BLOCK_SIZE 10240

typedef struct {
    struct archive *reader;
    struct archive *writer;
    struct cc_array_s *hardlinks;
    struct cc_array_s *whiteouts;
    struct cc_array_s *opaque_whiteouts;
} nsh_oci_unpack_context_t;

static int nsh_oci_unpack_context_init(nsh_oci_unpack_context_t *ctx,
    struct archive *reader, struct archive *writer)
{
    enum cc_stat cc_stat;

    ctx->reader = reader;
    ctx->writer = writer;

    cc_stat = cc_array_new(&ctx->hardlinks);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate hardlinks dynarray\n");
        goto error;
    }

    cc_stat = cc_array_new(&ctx->whiteouts);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate whiteouts dynarray\n");
        goto error_destroy_hardlinks;
    }

    cc_stat = cc_array_new(&ctx->opaque_whiteouts);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate opaque whiteouts dynarray\n");
        goto error_destroy_whiteouts;
    }

    return 0;

error_destroy_whiteouts:
    cc_array_destroy(ctx->whiteouts);
error_destroy_hardlinks:
    cc_array_destroy(ctx->hardlinks);
error:
    return -1;
}

static void nsh_oci_unpack_context_destroy(nsh_oci_unpack_context_t *ctx)
{
    cc_array_destroy_cb(ctx->opaque_whiteouts,
        (void (*)(void *))archive_entry_free);

    cc_array_destroy_cb(ctx->whiteouts,
        (void (*)(void *))archive_entry_free);

    cc_array_destroy_cb(ctx->hardlinks,
        (void (*)(void *))archive_entry_free);
}

static bool nsh_oci_entry_is_valid(struct archive_entry *entry)
{
    return !nsh_overlay_archive_entry_is_whiteout(entry) &&
           !nsh_overlay_archive_entry_is_opaque(entry);
}

static int nsh_oci_unpack_hardlinks(nsh_oci_unpack_context_t *ctx)
{
    struct archive_entry *entry;
    int retcode;

    for (size_t i = 0; i < cc_array_size(ctx->hardlinks); i++) {
        cc_array_get_at(ctx->hardlinks, i, (void **)&entry);

        retcode = archive_write_header(ctx->writer, entry);
        if (retcode != ARCHIVE_OK) {
            fprintf(stderr, "[x] Failed to write header of layer hardlink: %s\n",
                    archive_error_string(ctx->writer));
            return -1;
        }

        retcode = archive_write_finish_entry(ctx->writer);
        if (retcode != ARCHIVE_OK) {
            fprintf(stderr, "[x] Failed to finalize layer hardlink entry: %s\n",
                    archive_error_string(ctx->writer));
            return -1;
        }
    }

    return 0;
}

static int nsh_oci_unpack_whiteouts(nsh_oci_unpack_context_t *ctx)
{
    struct archive_entry *entry;
    const char *whiteout_path;
    char file_path[PATH_MAX];
    int err;

    for (size_t i = 0; i < cc_array_size(ctx->whiteouts); i++) {
        cc_array_get_at(ctx->whiteouts, i, (void **)&entry);

        whiteout_path = archive_entry_pathname(entry);
        err = nsh_oci_whiteout_to_file_new(whiteout_path, file_path, sizeof(file_path));
        if (err) {
            fprintf(stderr,
                "[x] Failed to resolve file path of whiteout: %s\n", whiteout_path);
            return -1;
        }

        err = nsh_overlay_whiteout_create(file_path);
        if (err)
            return -1;
    }

    return 0;
}

static int nsh_oci_unpack_opaque_whiteouts(nsh_oci_unpack_context_t *ctx)
{
    struct archive_entry *entry;
    const char *opaque_whiteout_path;
    char dir_path[PATH_MAX];
    int err;

    for (size_t i = 0; i < cc_array_size(ctx->opaque_whiteouts); i++) {
        cc_array_get_at(ctx->opaque_whiteouts, i, (void **)&entry);

        opaque_whiteout_path = archive_entry_pathname(entry);
        split_path(opaque_whiteout_path, dir_path, sizeof(dir_path), NULL, 0);

        err = nsh_overlay_opaque_set(dir_path);
        if (err)
            return -1;
    }

    return 0;
}

static int nsh_oci_unpack_content(
    struct archive *reader, struct archive *writer)
{
    const void *buf;
    size_t size;
    la_int64_t offset;
    int ret;

    while ((ret = archive_read_data_block(reader, &buf, &size, &offset)) == ARCHIVE_OK) {
        ret = archive_write_data_block(writer, buf, size, offset);
        if (ret != ARCHIVE_OK) {
            fprintf(stderr, "[x] Failed to write data block of layer entry: %s\n",
                    archive_error_string(writer));
            return -1;
        }
    }

    if (ret != ARCHIVE_EOF) {
        fprintf(stderr, "[x] Failed to read data block of layer entry: %s\n",
                archive_error_string(reader));
        return -1;
    }

    return 0;
}

static int nsh_oci_unpack_entry(
    struct archive_entry *entry, nsh_oci_unpack_context_t *ctx)
{
    int retcode;
    int err;

    retcode = archive_write_header(ctx->writer, entry);
    if (retcode != ARCHIVE_OK) {
        fprintf(stderr, "[x] Failed to write header of layer entry: %s\n",
                archive_error_string(ctx->writer));
        return -1;
    }

    err = nsh_oci_unpack_content(ctx->reader, ctx->writer);
    if (err)
        return -1;

    return 0;
}

static int nsh_oci_handle_hardlink(
    struct archive_entry *entry, nsh_oci_unpack_context_t *ctx)
{
    const char *pathname = archive_entry_pathname(entry);
    enum cc_stat cc_stat;

    cc_stat = cc_array_add_archive_entry(ctx->hardlinks, entry);
    if (cc_stat != CC_OK) {
        fprintf(stderr,
            "[x] Failed to add layer hardlink entry: %s\n", pathname);
        return -1;
    }

    return 0;
}

static int nsh_oci_handle_whiteout(
    struct archive_entry *entry, nsh_oci_unpack_context_t *ctx)
{
    const char *pathname = archive_entry_pathname(entry);
    enum cc_stat cc_stat;

    if (!nsh_oci_is_valid_whiteout_new(pathname)) {
        fprintf(stderr, "[x] Invalid whiteout: %s\n", pathname);
        return -1;
    }

    cc_stat = cc_array_add_archive_entry(ctx->whiteouts, entry);
    if (cc_stat != CC_OK) {
        fprintf(stderr,
            "[x] Failed to add layer whiteout entry: %s\n", pathname);
        return -1;
    }

    return 0;
}

static int nsh_oci_handle_opaque_whiteout(
    struct archive_entry *entry, nsh_oci_unpack_context_t *ctx)
{
    const char *pathname = archive_entry_pathname(entry);
    enum cc_stat cc_stat;

    cc_stat = cc_array_add_archive_entry(ctx->opaque_whiteouts, entry);
    if (cc_stat != CC_OK) {
        fprintf(stderr,
            "[x] Failed to add layer opaque whiteout entry: %s\n", pathname);
        return -1;
    }

    return 0;
}

static int nsh_oci_handle_entry(
    struct archive_entry *entry, nsh_oci_unpack_context_t *ctx)
{
    const char *pathname = archive_entry_pathname(entry);

    if (!nsh_oci_entry_is_valid(entry))
        return -1;

    if (archive_entry_hardlink(entry)) {
        return nsh_oci_handle_hardlink(entry, ctx);
    }
    if (nsh_oci_is_whiteout_new(pathname)) {
        return nsh_oci_handle_whiteout(entry, ctx);
    }
    if (nsh_oci_is_opaque_whiteout_new(pathname)) {
        return nsh_oci_handle_opaque_whiteout(entry, ctx);
    }

    return nsh_oci_unpack_entry(entry, ctx);
}

static int nsh_oci_unpack_entries(nsh_oci_unpack_context_t *ctx)
{
    struct archive_entry *entry;
    int err;
    int retcode;

    while ((retcode = archive_read_next_header(ctx->reader, &entry)) == ARCHIVE_OK) {
        err = nsh_oci_handle_entry(entry, ctx);
        if (err)
            return -1;

        retcode = archive_read_data_skip(ctx->reader);
        if (retcode != ARCHIVE_OK) {
            fprintf(stderr, "[x] Failed to skip entry in layer entry: %s\n",
                    archive_error_string(ctx->reader));
            return -1;
        }
    }

    if (retcode != ARCHIVE_EOF) {
        fprintf(stderr, "[x] Failed to read header of layer entry: %s\n",
                archive_error_string(ctx->reader));
        return -1;
    }

    return 0;
}

static int nsh_oci_unpack_internal(struct archive *reader, struct archive *writer)
{
    nsh_oci_unpack_context_t ctx;
    int err;
    int ret = 0;

    err = nsh_oci_unpack_context_init(&ctx, reader, writer);
    if (err) {
        ret = -1;
        goto out;
    }

    err = nsh_oci_unpack_entries(&ctx);
    if (err) {
        ret = -1;
        goto out_destroy_context;
    }

    err = nsh_oci_unpack_hardlinks(&ctx);
    if (err) {
        ret = -1;
        goto out_destroy_context;
    }

    err = nsh_oci_unpack_whiteouts(&ctx);
    if (err) {
        ret = -1;
        goto out_destroy_context;
    }

    err = nsh_oci_unpack_opaque_whiteouts(&ctx);
    if (err) {
        ret = -1;
        goto out_destroy_context;
    }

out_destroy_context:
    nsh_oci_unpack_context_destroy(&ctx);
out:
    return ret;
}

static int nsh_oci_unpack_archive(int fd, int format, int filter)
{
    struct archive *reader;
    struct archive *writer;
    int retcode;
    int err;
    int ret = 0;

    reader = archive_read_new();
    writer = archive_write_disk_new();

    archive_read_support_format_by_code(reader, format);
    archive_read_support_filter_by_code(reader, filter);

    archive_write_disk_set_options(
        writer,
        ARCHIVE_EXTRACT_OWNER   |
        ARCHIVE_EXTRACT_PERM    |
        ARCHIVE_EXTRACT_TIME    |
        ARCHIVE_EXTRACT_ACL     |
        ARCHIVE_EXTRACT_FFLAGS  |
        ARCHIVE_EXTRACT_XATTR
    );

    retcode = archive_read_open_fd(reader, fd,
        NSH_OCI_LAYER_UNPACK_BLOCK_SIZE);
    if (retcode != ARCHIVE_OK) {
        fprintf(stderr, "[x] Failed open archive reader: %s\n",
                archive_error_string(reader));
        ret = -1;
        goto out;
    }

    err = nsh_oci_unpack_internal(reader, writer);
    if (err) {
        ret = -1;
        goto out;
    }

out:
    archive_write_free(writer);
    archive_read_free(reader);
    return ret;
}

int nsh_oci_layer_unpack(const char *path, const char *target, int format, int filter)
{
    char *cwd;
    int fd;
    int err;
    int ret = 0;

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("[x] Failed to open layer archive");
        ret = -1;
        goto out;
    }

    err = mkdir(target, S_IRWXU);
    if (err) {
        perror("[x] Failed to create unpack target directory");
        ret = -1;
        goto out_close_layer;
    }

    cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        perror("[x] Failed to retrieve current working directory");
        ret = -1;
        goto out_close_layer;
    }

    err = chdir(target);
    if (err) {
        perror("[x] Failed to switch current working directory");
        ret = -1;
        goto out_close_layer;
    }

    err = nsh_oci_unpack_archive(fd, format, filter);
    if (err) {
        ret = -1;
        goto out_restore_cwd;
    }

out_restore_cwd:
    err = chdir(cwd);
    if (err) {
        perror("[x] Failed to restore current working directory");
        ret = -1;
    }
    free(cwd);
out_close_layer:
    close(fd);
out:
    return ret;
}

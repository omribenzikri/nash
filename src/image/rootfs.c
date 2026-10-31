#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <archive.h>
#include <archive_entry.h>
#include <collectc/cc_common.h>
#include <collectc/cc_array.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_content_descriptor.h>
#include "common/constants.h"
#include "utils/filesystem.h"
#include "image/oci.h"
#include "image/rootfs.h"

#define NSH_LAYER_EXTRACTION_BLOCK_SIZE 10240

static int nsh_layer_digest_to_snapshot_path(
    int snapshots_dir_fd, const char *digest, char *path, size_t size)
{
    int snapshot_fd;
    int err;
    snapshot_fd = openat(snapshots_dir_fd, digest, O_RDONLY | O_DIRECTORY);
    if (snapshot_fd == -1) {
        perror("[x] Failed to open snapshot directory");
        return -1;
    }

    err = fd_to_pathname(snapshot_fd, path, size);
    if (err) {
        perror("[x] Failed to convert file descriptor to pathname");
        return -1;
    }

    return 0;
}

static int nsh_pending_hardlinks_add(
    struct cc_array_s *arr, struct archive_entry *entry)
{
    struct archive_entry *cloned;
    enum cc_stat cc_stat;

    cloned = archive_entry_clone(entry);
    if (cloned == NULL) {
        fprintf(stderr, "[x] Failed to clone hardlink archive entry\n");
        goto error;
    }

    cc_stat = cc_array_add(arr, cloned);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to insert pending hardlink archive entry\n");
        goto error_free_entry;
    }

    return 0;

error_free_entry:
    archive_entry_free(cloned);
error:
    return -1;
}

static int nsh_pending_hardlinks_extract(
    struct cc_array_s *arr, struct archive *writer)
{
    struct archive_entry *entry;
    enum cc_stat cc_stat;
    int retcode;

    for (size_t i = 0; i < cc_array_size(arr); i++) {
        cc_stat = cc_array_get_at(arr, i, (void **)&entry);
        assert(cc_stat == CC_OK);

        retcode = archive_write_header(writer, entry);
        if (retcode != ARCHIVE_OK) {
            fprintf(stderr, "[x] Failed to write header of layer archive: %s\n",
                    archive_error_string(writer));
            return -1;
        }

        retcode = archive_write_finish_entry(writer);
        if (retcode != ARCHIVE_OK) {
            fprintf(stderr, "[x] Failed to finalize layer archive entry: %s\n",
                    archive_error_string(writer));
            return -1;
        }
    }

    return 0;
}

static void nsh_layer_archive_entry_set_dest(
    struct archive_entry *entry, const char *snapshot_path)
{
    const char *path;
    char buf[PATH_MAX];

    path = archive_entry_pathname(entry);
    snprintf(buf, sizeof(buf), "%s/%s", snapshot_path, path);
    archive_entry_set_pathname(entry, buf);

    path = archive_entry_hardlink(entry);
    if (path != NULL) {
        snprintf(buf, sizeof(buf), "%s/%s", snapshot_path, path);
        archive_entry_set_hardlink(entry, buf);
    }
}

static int nsh_layer_extract_copy_data_blocks(
    struct archive *reader, struct archive *writer)
{
    const void *buf;
    size_t size;
    la_int64_t offset;
    int ret;

    while ((ret = archive_read_data_block(reader, &buf, &size, &offset)) == ARCHIVE_OK) {
        ret = archive_write_data_block(writer, buf, size, offset);
        if (ret != ARCHIVE_OK) {
            fprintf(stderr, "[x] Failed to write data block of layer archive: %s\n",
                    archive_error_string(writer));
            return -1;
        }
    }

    if (ret != ARCHIVE_EOF) {
        fprintf(stderr, "[x] Failed to read data block of layer archive: %s\n",
                archive_error_string(reader));
        return -1;
    }

    return 0;
}

static int nsh_layer_extract_archive_entry_single(
    struct archive *reader, struct archive *writer, struct archive_entry *entry)
{
    int retcode;
    int err;

    retcode = archive_write_header(writer, entry);
    if (retcode != ARCHIVE_OK) {
        fprintf(stderr, "[x] Failed to write header of layer archive: %s\n",
                archive_error_string(writer));
        return -1;
    }

    // Whiteouts are not handled
    err = nsh_layer_extract_copy_data_blocks(reader, writer);
    if (err)
        return -1;

    return 0;
}

static int nsh_layer_extract_archive_entries(
    struct archive *reader, struct archive *writer, const char *snapshot_path)
{
    struct archive_entry *entry;
    struct cc_array_s *pending_hardlinks;
    enum cc_stat cc_stat;
    int ret = 0;
    int retcode;
    int err;

    cc_stat = cc_array_new(&pending_hardlinks);
    if (cc_stat != CC_OK) {
        fprintf(stderr, "[x] Failed to allocate pending hardlinks array\n");
        ret = -1;
        goto out;
    }

    while ((retcode = archive_read_next_header(reader, &entry)) == ARCHIVE_OK) {
        nsh_layer_archive_entry_set_dest(entry, snapshot_path);

        if (archive_entry_hardlink(entry)) {
            err = nsh_pending_hardlinks_add(pending_hardlinks, entry);
            if (err) {
                ret = -1;
                goto out_destroy_arr;
            }
        }
        else {
            err = nsh_layer_extract_archive_entry_single(reader, writer, entry);
            if (err) {
                ret = -1;
                goto out_destroy_arr;
            }
        }

        retcode = archive_read_data_skip(reader);
        if (retcode != ARCHIVE_OK) {
            fprintf(stderr, "[x] Failed to skip entry in layer archive: %s\n",
                    archive_error_string(writer));
            ret = -1;
            goto out_destroy_arr;
        }
    }

    if (retcode != ARCHIVE_EOF) {
        fprintf(stderr, "[x] Failed to read header of layer archive: %s\n",
                archive_error_string(reader));
        ret = -1;
        goto out_destroy_arr;
    }

    err = nsh_pending_hardlinks_extract(pending_hardlinks, writer);
    if (err) {
        ret = -1;
        goto out_destroy_arr;
    }

out_destroy_arr:
    cc_array_destroy_cb(pending_hardlinks,
        (void (*)(void *))archive_entry_free);
out:
    return ret;
}

static int nsh_layer_extract_archive(const char *layer_path, const char *snapshot_path)
{
    struct archive *reader;
    struct archive *writer;
    int retcode;
    int err;
    int ret = 0;

    reader = archive_read_new();
    writer = archive_write_disk_new();

    archive_read_support_format_all(reader);
    archive_read_support_filter_all(reader);

    archive_write_disk_set_options(
        writer,
        ARCHIVE_EXTRACT_OWNER   |
        ARCHIVE_EXTRACT_PERM    |
        ARCHIVE_EXTRACT_TIME    |
        ARCHIVE_EXTRACT_ACL     |
        ARCHIVE_EXTRACT_FFLAGS  |
        ARCHIVE_EXTRACT_XATTR
    );

    retcode = archive_read_open_filename(
        reader, layer_path, NSH_LAYER_EXTRACTION_BLOCK_SIZE);
    if (retcode != ARCHIVE_OK) {
        fprintf(stderr, "[x] Failed open layer archive: %s\n",
                archive_error_string(reader));
        ret = -1;
        goto out;
    }

    err = nsh_layer_extract_archive_entries(reader, writer, snapshot_path);
    if (err) {
        ret = -1;
        goto out;
    }

out:
    archive_write_free(writer);
    archive_read_free(reader);
    return ret;
}

static int nsh_layer_extract_single(const char *image_path, int snapshots_dir_fd,
    image_spec_schema_content_descriptor *layer_descriptor)
{
    char layer_path[PATH_MAX];
    char snapshot_path[PATH_MAX];
    int err;

    err = mkdirat(snapshots_dir_fd, layer_descriptor->digest, S_IRWXU);
    if (err) {
        perror("[x] Failed to create snapshot directory");
        return -1;
    }

    err = nsh_oci_digest_to_path(image_path, layer_descriptor->digest,
                                 layer_path, sizeof(layer_path));
    if (err) {
        fprintf(stderr, "[x] Failed to resolve layer blob path by digest\n");
        return -1;
    }

    err = nsh_layer_digest_to_snapshot_path(snapshots_dir_fd, layer_descriptor->digest,
                                            snapshot_path, sizeof(snapshot_path));
    if (err) {
        fprintf(stderr, "[x] Failed to resolve snapshot path by digest\n");
        return -1;
    }

    err = nsh_layer_extract_archive(layer_path, snapshot_path);
    if (err)
        return -1;

    return 0;
}

int nsh_image_layers_extract(
    image_spec_schema_image_manifest_schema *manifest,
    const char *image_path,
    int workdir_fd)
{
    image_spec_schema_content_descriptor *layer_descriptor;
    int snapshots_dir_fd;
    int ret = 0;
    int err;

    snapshots_dir_fd = openat(workdir_fd,
        NSH_FS_WORKDIR_OVERLAY_SNAPSHOTS, O_RDONLY | O_DIRECTORY);

    if (snapshots_dir_fd == -1) {
        perror("[x] Failed to open snapshots directory");
        ret = -1;
        goto out;
    }

    for (size_t i = 0; i < manifest->layers_len; i++) {
        layer_descriptor = manifest->layers[i];

        err = nsh_layer_extract_single(
            image_path, snapshots_dir_fd, layer_descriptor);

        if (err) {
            ret = -1;
            goto out_close_snapshots_dir;
        }
    }

out_close_snapshots_dir:
    err = close(snapshots_dir_fd);
    if (err) {
        perror("[x] Failed to close snapshots directory");
        ret = -1;
    }
out:
    return ret;
}

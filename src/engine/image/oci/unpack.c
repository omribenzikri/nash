#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <archive.h>
#include <archive_entry.h>
#include "engine/image/oci/unpack.h"

#define NSH_OCI_LAYER_UNPACK_BLOCK_SIZE 10240

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

static int nsh_oci_unpack_entry(struct archive_entry *entry,
    struct archive *reader, struct archive *writer)
{
    int retcode;
    int err;

    retcode = archive_write_header(writer, entry);
    if (retcode != ARCHIVE_OK) {
        fprintf(stderr, "[x] Failed to write header of layer entry: %s\n",
                archive_error_string(writer));
        return -1;
    }

    err = nsh_oci_unpack_content(reader, writer);
    if (err)
        return -1;

    return 0;
}

static int nsh_oci_handle_entry(struct archive_entry *entry,
    struct archive *reader, struct archive *writer)
{
    // TODO: verify entry validity
    // TODO: handle hardlink
    // TODO: handle whiteout
    // TODO: handle opaque whiteout
    if (archive_entry_hardlink(entry))
        return 0;

    return nsh_oci_unpack_entry(entry, reader, writer);
}

static int nsh_oci_unpack_entries(struct archive *reader, struct archive *writer)
{
    struct archive_entry *entry;
    int retcode;
    int err;

    while ((retcode = archive_read_next_header(reader, &entry)) == ARCHIVE_OK) {
        err = nsh_oci_handle_entry(entry, reader, writer);
        if (err)
            return -1;

        retcode = archive_read_data_skip(reader);
        if (retcode != ARCHIVE_OK) {
            fprintf(stderr, "[x] Failed to skip entry in layer entry: %s\n",
                    archive_error_string(writer));
            return -1;
        }
    }

    if (retcode != ARCHIVE_EOF) {
        fprintf(stderr, "[x] Failed to read header of layer entry: %s\n",
                archive_error_string(reader));
        return -1;
    }

    return 0;
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

    err = nsh_oci_unpack_entries(reader, writer);
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

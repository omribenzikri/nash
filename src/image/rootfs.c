#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <archive.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_content_descriptor.h>
#include "common/constants.h"
#include "image/oci.h"
#include "image/rootfs.h"

#define NSH_LAYER_EXTRACTION_BLOCK_SIZE 10240

static int nsh_image_layer_extract_archive(const char *layer_path)
{
    struct archive *reader;
    struct archive *writer;
    int retcode;
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

out:
    archive_write_free(writer);
    archive_read_free(reader);
    return ret;
}

static int nsh_image_layer_extract_single(const char *image_path, int layers_dir_fd,
    image_spec_schema_content_descriptor *layer_descriptor)
{
    char layer_path[PATH_MAX];
    int err;

    err = nsh_oci_digest_to_path(image_path, layer_descriptor->digest,
        layer_path, sizeof(layer_path));
    if (err) {
        fprintf(stderr, "[x] Failed to resolve layer blob path by digest\n");
        return -1;
    }

    err = mkdirat(layers_dir_fd, layer_descriptor->digest, S_IRWXU);
    if (err) {
        perror("[x] Failed to create layer directory");
        return -1;
    }

    err = nsh_image_layer_extract_archive(layer_path);
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
    int layers_dir_fd;
    int ret = 0;
    int err;

    layers_dir_fd = openat(workdir_fd,
        NSH_FS_WORKDIR_OVERLAY_LAYERS, O_RDONLY | O_DIRECTORY);

    if (layers_dir_fd == -1) {
        perror("[x] Failed to open layers directory");
        ret = -1;
        goto out;
    }

    for (size_t i = 0; i < manifest->layers_len; i++) {
        layer_descriptor = manifest->layers[i];

        err = nsh_image_layer_extract_single(
            image_path, layers_dir_fd, layer_descriptor);

        if (err) {
            ret = -1;
            goto out_close_layers_dir;
        }
    }

out_close_layers_dir:
    err = close(layers_dir_fd);
    if (err) {
        perror("[x] Failed to close layers directory");
        ret = -1;
    }
out:
    return ret;
}

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_content_descriptor.h>
#include "common/constants.h"
#include "image/rootfs.h"

int nsh_image_layers_extract(
    image_spec_schema_image_manifest_schema *manifest,
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

        err = mkdirat(layers_dir_fd, layer_descriptor->digest, S_IRWXU);
        if (err) {
            perror("[x] Failed to create layer directory");
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

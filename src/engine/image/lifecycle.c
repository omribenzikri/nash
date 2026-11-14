#include <stdio.h>
#include "common/constants.h"
#include "common/utils/filesystem.h"
#include "engine/image/oci.h"
#include "engine/image/lifecycle.h"

// Assuming the top-level workdir exists
static int nsh_image_store_create_if_missing()
{
    int err;

    err = mkdir_if_missing(NSH_STORE_SNAPSHOTS, S_IRWXU);
    if (err) {
        perror("[x] Failed to create " NSH_STORE_SNAPSHOTS);
        return -1;
    }

    err = mkdir_if_missing(NSH_STORE_MANIFESTS, S_IRWXU);
    if (err) {
        perror("[x] Failed to create " NSH_STORE_MANIFESTS);
        return -1;
    }

    err = mkdir_if_missing(NSH_STORE_CONFIG, S_IRWXU);
    if (err) {
        perror("[x] Failed to create " NSH_STORE_CONFIG);
        return -1;
    }

    return 0;
}

static int nsh_image_import_blobs(const char *path)
{
    nsh_oci_image_meta_t image_meta;
    int err;
    int ret = 0;

    err = nsh_oci_image_meta_load(&image_meta, path);
    if (err) {
        ret = -1;
        goto out;
    }

    puts(image_meta.ref);
    goto out_destroy_image_meta;

out_destroy_image_meta:
    nsh_oci_image_meta_destroy(&image_meta);
out:
    return ret;
}

int nsh_image_import(const char *path)
{
    int err;

    err = nsh_image_store_create_if_missing();
    if (err)
        return -1;

    err = nsh_image_import_blobs(path);
    if (err)
        return -1;

    return 0;
}

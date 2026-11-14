#include <stdio.h>
#include "engine/image/oci.h"
#include "engine/image/lifecycle.h"

int nsh_image_import(const char *path)
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

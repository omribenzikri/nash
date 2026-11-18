#include <ocispec/image_spec_schema_image_manifest_schema.h>
#include <ocispec/image_spec_schema_config_schema.h>
#include "engine/image/oci/common.h"

void nsh_oci_image_meta_destroy(nsh_oci_image_meta_t *meta)
{
    free_image_spec_schema_image_manifest_schema(meta->manifest);
    free_image_spec_schema_config_schema(meta->config);
}

#ifndef _NASH_ENGINE_IMAGE_OCI_LIFECYCLE_H
#define _NASH_ENGINE_IMAGE_OCI_LIFECYCLE_H

int nsh_oci_image_import(const char *store_path, const char *bundle_path, char **out);
int nsh_oci_image_delete(const char *store_path, const char *ref);

#endif

#include <stddef.h>
#include <string.h>
#include <archive.h>
#include "common/utils/archive.h"

bool archive_entry_xattr_exists(struct archive_entry *entry, const char *name)
{
    const char *xname;
    const void *xvalue;
    size_t xsize;
    int retcode;

    archive_entry_xattr_reset(entry);

    while ((retcode = archive_entry_xattr_next(
        entry, &xname, &xvalue, &xsize)) == ARCHIVE_OK) {
            if (strcmp(xname, name) == 0)
                return true;
        }

    return false;
}

#ifndef _NASH_UTILS_ARCHIVE_H
#define _NASH_UTILS_ARCHIVE_H

#include <stdbool.h>
#include <archive_entry.h>

bool archive_entry_xattr_exists(struct archive_entry *entry, const char *name);

#endif

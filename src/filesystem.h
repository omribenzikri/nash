#ifndef _NASH_FILESYSTEM_H
#define _NASH_FILESYSTEM_H

int nsh_pivot_root(const char *root);
int nsh_mount_essentials();
int nsh_populate_dev();

#endif

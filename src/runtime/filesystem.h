#ifndef _NASH_RUNTIME_FILESYSTEM_H
#define _NASH_RUNTIME_FILESYSTEM_H

int nsh_pivot_root(const char *root);
int nsh_mount_essentials(void);
int nsh_populate_dev(void);

#endif

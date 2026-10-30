#ifndef _NASH_RUNTIME_FILESYSTEM_H
#define _NASH_RUNTIME_FILESYSTEM_H

int nsh_fs_pivot_root(const char *root);
int nsh_fs_mount_essentials(void);
int nsh_fs_populate_dev(void);

#endif

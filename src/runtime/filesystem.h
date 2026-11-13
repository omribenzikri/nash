#ifndef _NASH_RUNTIME_FILESYSTEM_H
#define _NASH_RUNTIME_FILESYSTEM_H

int nsh_fs_mount(void);
int nsh_fs_populate(void);
int nsh_fs_pivot_root(const char *root, const char *cwd);

#endif

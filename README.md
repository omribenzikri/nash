## Nash
Linux container engine POC in the works, capable of:
1. Cloning a process in separate mount, PID, UTS and IPC namespaces
2. Pivoting its root to an extracted root filesystem
3. Mounting procfs, sysfs and populating a minimal `/dev`

#### Dependencies
* `libuuid`

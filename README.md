## Nash
Linux container engine POC in the works, capable of:
1. Cloning a process in separate mount, PID, UTS, IPC and cgroup namespaces
2. Pivoting its root to an extracted root filesystem
3. Mounting procfs, sysfs and populating a minimal `/dev`
4. Restricting container CPU time, processes, memory and swap via cgroupv2

#### Dependencies
* `libuuid`

## Nash
Linux container engine POC in the works.

### Working Features

##### Runtime
1. Cloning a process in separate mount, PID, UTS, IPC and cgroup namespaces
2. Pivoting its root to an extracted root filesystem
3. Mounting procfs, sysfs and populating a minimal `/dev`
4. Restricting container CPU time, processes, memory and swap via cgroupv2
5. Whitelisting capabilities

##### Engine
1. Parsing OCI image metadata
2. Unpacking layers in order, and applying whiteouts
3. Merging layers via overlayfs mount
4. Applying image config + user overrides

#### Dependencies
* `libuuid`
* `libcap`
* `libjson-c`
* `libocispec`
* `libsodium`
* `libblake3`
* `libarchive`
* `libcollectc`

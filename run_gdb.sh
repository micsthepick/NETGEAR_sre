gdb-multiarch -nx -ex "set sysroot $(pwd)/squashfs_root_bb/" -ex "file squashfs_root_bb/lib/libc.so" -ex "target remote :241"


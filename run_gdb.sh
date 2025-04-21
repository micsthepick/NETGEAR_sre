gdb-multiarch -ex "set sysroot $(pwd)/squashfs_root_bb/" -ex "target remote :241" -ex "file squashfs_root_bb/fw_hacks.so"

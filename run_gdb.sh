test ! -n "$NOGDBINIT" && NOINIT="-nx"
gdb-multiarch $NOINIT -ex "set sysroot $(pwd)/squashfs_root_bb/" -ex "file squashfs_root_bb/lib/libc.so" -ex "target remote :241" -ex "catch load /fw_hacks.so" -ex "c"


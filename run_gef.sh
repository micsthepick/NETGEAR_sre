test ! -n "$NOGDBINIT" && NOINIT="-nx"
gdb-multiarch $NOINIT -ex "source gef.py" -ex "set sysroot $(pwd)/squashfs_root_bb/" -ex "file squashfs_root_bb/lib/libc.so" -ex "gef-remote --qemu-user --qemu-binary squashfs_root_bb/usr/bin/env localhost 241" -ex "catch load /fw_hacks.so" -ex "c"


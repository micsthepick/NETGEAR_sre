sudo gdb-multiarch -nx -x pre_debug.gdb -ex "source gef.py" -ex "gef-remote --qemu-user --qemu-binary squashfs_root_bb/lib/libc.so localhost 241" -x jump_to_start.gdb $@

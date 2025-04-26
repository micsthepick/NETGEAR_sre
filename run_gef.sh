gdb-multiarch -nx -ex "source gef.py" -ex "gef-remote --qemu-user --qemu-binary squashfs_root_bb/usr/bin/env localhost 241" -x jump_to_start.gdb $@

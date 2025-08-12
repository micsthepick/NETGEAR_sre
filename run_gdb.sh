test ! -z "$NOGDBINIT" && NOINIT="-nx"
sudo gdb-multiarch $NOINIT -x pre_debug.gdb -ex "target remote :241" -x jump_to_start.gdb $@


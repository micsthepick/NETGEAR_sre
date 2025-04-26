test ! -z "$NOGDBINIT" && NOINIT="-nx"
gdb-multiarch $NOINIT -ex "target remote :241" -ex jump_to_start.gdb $@


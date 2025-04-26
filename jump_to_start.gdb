set sysroot squashfs_root_bb/
file squashfs_root_bb/lib/libc.so
set breakpoint pending on
catch load /fw_hacks.so
continue
break main_hook
continue
break *real_main
continue
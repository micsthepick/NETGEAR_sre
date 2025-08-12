file squashfs_root_bb/lib/libc.so
set breakpoint pending on
set auto-solib-add on

catch load /fw_hacks.so
continue
delete breakpoint
break main_hook
continue
delete breakpoint
break main_hook
#continue
#delete breakpoint
#break *real_main
#continue
#delte breakpoint

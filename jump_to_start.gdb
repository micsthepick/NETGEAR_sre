file squashfs_root_bb/lib/libc.so
set breakpoint pending on
set auto-solib-add on
set follow-fork-mode parent
set detach-on-fork off 
set schedule-multiple on

catch load /fw_hacks.so
continue
delete breakpoint
break main_hook
continue
#continue
#delete breakpoint
#break *real_main
#continue
#delte breakpoint

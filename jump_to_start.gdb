file squashfs_root_bb/lib/libc.so
set breakpoint pending on
set auto-solib-add on

#break __libc_start_main
#continue
#delete breakpoint
catch load /fw_hacks.so
#continue
#break main_hook
#continue
#delete breakpoint
#break main_hook
#continue
#delete breakpoint
#break *real_main

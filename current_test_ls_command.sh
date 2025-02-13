./compile_fw_hacks.sh && sudo cp fw_hacks.so busybox_install/ && sudo chroot squashfs_root_bb env QEMU_STRACE= LD_PRELOAD=/fw_hacks.so /bin/ls

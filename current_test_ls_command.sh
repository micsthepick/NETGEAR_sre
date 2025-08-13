#!/bin/bash
. ./common.sh

must ./compile_fw_hacks.sh
must sudo cp fw_hacks.so busybox_install/ 
must sudo chroot squashfs_root_bb env QEMU_STRACE= LD_PRELOAD=/fw_hacks.so /bin/ls

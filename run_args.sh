sudo systemctl start systemd-binfmt
[[ $QS -eq 1 ]] && CHROOTENV+=" QEMU_STRACE="
sudo chroot squashfs_root_bb /usr/bin/env -i HOME=/ PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin "SHELL=/bin/sh" $CHROOTENV LD_PRELOAD=/fw_hacks.so $@

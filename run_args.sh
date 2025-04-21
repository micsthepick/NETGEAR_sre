systemctl start systemd-binfmt
test -n "$QS" && export CHROOTENV="$CHROOTENV QEMU_STRACE="
test -n "$QD" && export QEMUDBG=" qemu-arm-static -g 241 "
chroot squashfs_root_bb /usr/bin/env -i HOME=/ PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin "SHELL=/bin/sh" $CHROOTENV LD_PRELOAD=/fw_hacks.so $QEMUDBG $@

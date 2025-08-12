systemctl start systemd-binfmt
test -n "$LOUD" && export CHROOTENV="$CHROOTENV FHACKS_NOISE=1"
test -n "$QS" && export CHROOTENV="$CHROOTENV QEMU_STRACE=1"
test -n "$QD" && export QEMUDBG="-g 241"
sudo env -i HOME=/ PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin SHELL=/bin/sh $CHROOTENV chroot squashfs_root_bb qemu-arm-static $QEMUDBG /usr/bin/env LD_PRELOAD=/fw_hacks.so $@


sudo env SHELL=/bin/sh PATH=/usr/bin:/usr/sbin:/sbin:/bin chroot squashfs_root_bb /bin/mknod -m 666 /dev/null c 1 3
sudo env SHELL=/bin/sh PATH=/usr/bin:/usr/sbin:/sbin:/bin chroot squashfs_root_bb /bin/mknod /module_name c 1 3
echo -n "Base" | sudo tee squashfs_root_bb/tmp/orbi_type >/dev/null
test ! -e squashfs_root_bb/lib/libc.so.bak && sudo mv squashfs_root_bb/lib/libc.so squashfs_root_bb/lib/libc.so.bak
sudo cp arm-unknown-linux-musleabi/arm-unknown-linux-musleabi/sysroot/usr/lib/libc.so squashfs_root_bb/lib/libc.so
echo -e "nameserver 8.8.8.8\nnameserver 8.8.4.4" > squashfs_root_bb/tmp/resolv.conf


test ! -e busybox-arm5l && sudo wget https://busybox.net/downloads/binaries/1.31.0-defconfig-multiarch-musl/busybox-armv5l
sudo cp busybox-arm5l squashfs_root_bb/bin/busybox
sudo cp $(which qemu-arm-static) squashfs_root_bb/$(which qemu-arm-static)
sudo env SHELL=/bin/sh PATH=/usr/bin:/usr/sbin:/sbin:/bin chroot squashfs_root_bb /bin/mknod -m 666 /dev/null c 1 3
echo -n "Base" | sudo tee squashfs_root_bb/tmp/orbi_type >/dev/null
test ! -e squashfs_root_bb/lib/libc.so.bak && sudo mv squashfs_root_bb/lib/libc.so squashfs_root_bb/lib/libc.so.bak
sudo cp arm-unknown-linux-musleabi/arm-unknown-linux-musleabi/sysroot/usr/lib/libc.so squashfs_root_bb/lib/libc.so
echo -e "nameserver 8.8.8.8\nnameserver 8.8.4.4" | sudo tee squashfs_root_bb/tmp/resolv.conf
sudo rm -rf squashfs_root_bb/tmp
sudo mkdir -m 0777 squashfs_root_bb/tmp
# make temp not sticky
sudo mkdir squashfs_root_bb/tmp/run/
sudo chmod 777 squashfs_root_bb/tmp/run
sudo mkdir -p squashfs_root_bb/tmp/log/lighttpd
sudo chmod 777 squashfs_root_bb/tmp/log
sudo mkdir -p squashfs_root_bb/tmp/cache/gui
sudo chmod 777 squashfs_root_bb/tmp/cache

sudo ln -s /run/systemd/journal/dev-log squashfs_root_bb/dev/log

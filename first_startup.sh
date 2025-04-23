if ! sudo echo first startup
then
    echo must run with root
fi
test ! -e busybox-armv5l && sudo wget https://busybox.net/downloads/binaries/1.31.0-defconfig-multiarch-musl/busybox-armv5l
sudo cp busybox-armv5l squashfs_root_bb/bin/busybox
sudo cp $(which qemu-arm-static) squashfs_root_bb/$(which qemu-arm-static)
(sudo env SHELL=/bin/sh PATH=/usr/bin:/usr/sbin:/sbin:/bin chroot squashfs_root_bb /bin/mknod -m 666 /dev/null c 1 3) 2> /dev/null
echo -n "Base" | sudo tee squashfs_root_bb/tmp/orbi_type > /dev/null
test ! -e squashfs_root_bb/lib/libc.so.bak && sudo mv squashfs_root_bb/lib/libc.so squashfs_root_bb/lib/libc.so.bak
sudo cp arm-unknown-linux-musleabi/arm-unknown-linux-musleabi/sysroot/usr/lib/libc.so squashfs_root_bb/lib/libc.so
echo -e "nameserver 8.8.8.8\nnameserver 8.8.4.4" | sudo tee squashfs_root_bb/tmp/resolv.conf > /dev/null
sudo rm -rf squashfs_root_bb/tmp
sudo mkdir -m 0777 squashfs_root_bb/tmp
# make temp not sticky
sudo mkdir squashfs_root_bb/tmp/run/
sudo chmod 777 squashfs_root_bb/tmp/run
sudo mkdir -p squashfs_root_bb/tmp/log/lighttpd
sudo chmod 777 squashfs_root_bb/tmp/log
sudo mkdir -p squashfs_root_bb/tmp/cache/gui
sudo chmod 777 squashfs_root_bb/tmp/cache
sudo touch squashfs_root_bb/tmp/cache/config_part

echo -n "NAND" | sudo tee squashfs_root_bb/mtd > /dev/null

sudo rm -rf squashfs_root_bb/sys/block/mmcblk0/

sudo mkdir -p squashfs_root_bb/sys/block/mmcblk0/mmcblk0p1

addpath () {
    echo -e -n "DEVNAME=$2\0\nPARTNAME=$1\n" | sudo tee -a squashfs_root_bb/sys/block/mmcblk0/mmcblk0p1/uevent > /dev/null
    sudo mkdir -p squashfs_root_bb/dev/$2
}

for part in \
0:APPSBL 0:APPSBLENV 0:ART 0:ART.bak config config.bak boarddata1 boarddata1.bak boarddata2 boarddata2.bak dnidata \
firmware kernel rootfs firmware2 kernel2 rootfs2 language cert ntgrdata traffic_meter oopsdump pot pot.bak rae \
vol_traffic vol_traffic.bak vol_oopsdump vol_rae vol_circle vol_ntgr vol_armor sstorage
 do
    addpath $part mtdpath/$part/
done
sudo rm squashfs_root_bb/dev/console
sudo touch squashfs_root_bb/dev/console
sudo chmod 666 squashfs_root_bb/dev/console
sudo ln -s /run/systemd/journal/dev-log squashfs_root_bb/dev/log 2> /dev/null


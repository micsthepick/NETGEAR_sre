#!/bin/bash
. ./common.sh

if ! sudo echo first startup
then
    echo must run with root
    exit 1
fi
must ./install_dynamic_busybox.sh
must sudo cp $(which qemu-arm-static) squashfs_root_bb/$(which qemu-arm-static)
must sudo env SHELL=/bin/sh chroot squashfs_root_bb /bin/mknod -m 666 /dev/null c 1 3
must echo -n "Base" | sudo tee squashfs_root_bb/tmp/orbi_type > /dev/null
test ! -e squashfs_root_bb/lib/libc.so.bak && sudo mv squashfs_root_bb/lib/libc.so squashfs_root_bb/lib/libc.so.bak
must sudo cp arm-unknown-linux-musleabi/arm-unknown-linux-musleabi/sysroot/usr/lib/libc.so squashfs_root_bb/lib/libc.so
echo -e "nameserver 8.8.8.8\nnameserver 8.8.4.4" | sudo tee squashfs_root_bb/tmp/resolv.conf > /dev/null
must sudo rm -rf squashfs_root_bb/tmp
must sudo mkdir -m 0777 squashfs_root_bb/tmp
# make temp not sticky
must sudo mkdir squashfs_root_bb/tmp/run/
must sudo chmod 777 squashfs_root_bb/tmp/run
must sudo mkdir -p squashfs_root_bb/tmp/log/lighttpd
must sudo chmod 777 squashfs_root_bb/tmp/log
must sudo mkdir -p squashfs_root_bb/tmp/cache/gui
must sudo chmod 777 squashfs_root_bb/tmp/cache
must sudo touch squashfs_root_bb/tmp/cache/config_part

must sudo rm -rf squashfs_root_bb/sys/block/mmcblk0/

must sudo mkdir -p squashfs_root_bb/sys/block/mmcblk0/mmcblk0p1

addpath () {
    printf 'DEVNAME=%s/\nPARTNAME=%s\n' "$2" "$1" | sudo tee -a squashfs_root_bb/sys/block/mmcblk0/mmcblk0p1/uevent > /dev/null
}

must mkdir -p squashfs_root_bb/mtd

addpart () {
    echo "mtd$2:\"$1\"" >> squashfs_root_bb/mtd/info
}

echo "dev:\"name\"" > squashfs_root_bb/mtd/info

i=0
for part in \
0:APPSBL 0:APPSBLENV 0:ART 0:ART.bak config config.bak boarddata1 boarddata1.bak boarddata2 boarddata2.bak dnidata \
firmware kernel rootfs firmware2 kernel2 rootfs2 language cert ntgrdata traffic_meter oopsdump pot pot.bak rae \
vol_traffic vol_traffic.bak vol_oopsdump vol_rae vol_circle vol_ntgr vol_armor sstorage
do
    must addpath $part mtdpath/$part
    must addpart $part $i
    i=$((i+1))
done
must sudo rm squashfs_root_bb/dev/console
must sudo touch squashfs_root_bb/dev/console
must sudo chmod 666 squashfs_root_bb/dev/console
must sudo ln -s /run/systemd/journal/dev-log squashfs_root_bb/dev/log 2> /dev/null

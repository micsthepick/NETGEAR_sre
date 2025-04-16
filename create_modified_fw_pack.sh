sudo cp -P -r squashfs-root/ squashfs_root_bb
sudo cp -P -r busybox_install/lib squashfs_root_bb/lib
sudo cp busybox_install/bin/busybox squashfs_root_bb/bin/busybox
sudo rm -rf squashfs_root_bb/tmp
sudo mkdir squashfs_root_bb/tmp
sudo mkdir squashfs_root_bb/tmp/run
echo -n "Base" | sudo tee squashfs_root_bb/tmp/orbi_type >/dev/null

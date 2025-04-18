sudo mkdir -p squashfs_root_bb
sudo tar xf squashfs.tar.gz --strip-components 1 -C squashfs_root_bb
sudo ./mounts_for_fw_pack.sh
sudo ./first_startup.sh
sudo ./cc.sh

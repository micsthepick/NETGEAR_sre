if [ -e squashfs.tar.gz ]
then
    sudo mkdir -p squashfs_root_bb
    sudo tar xf squashfs.tar.gz -C squashfs_root_bb
    sudo ./mounts_for_fw_pack.sh
    sudo ./first_startup.sh
    sudo ./cc.sh
else
    echo Create squashfs.tar.gz first!
fi

if [ -e squashfs.tar ]
then
    sudo rm -rf squashfs_root_bb/dev/shm
    for fs in dev/random /proc dev/urandom run/systemd/journal
    do
        if [ -e squashfs_root_bb/$fs ]
        then
            sudo umount squashfs_root_bb/$fs
            sudo rm -r squashfs_root_bb/$fs
        fi
    done
    sudo rm -rf squashfs_root_bb
    sudo mkdir -p squashfs_root_bb
    sudo tar xf squashfs.tar -C squashfs_root_bb
    sudo ./mounts_for_fw_pack.sh
    sudo ./first_startup.sh
    sudo ./cc.sh
else
    echo Create squashfs.tar first!
fi

if [ ! -e squashfs_root_bb/dev/fw_hacks_con ]
then
    echo "looking for squashfs_root_bb/dev/fw_hacks_con"
    exit 1
fi
while true; do cat squashfs_root_bb/dev/fw_hacks_con; done

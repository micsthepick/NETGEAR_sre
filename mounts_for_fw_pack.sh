sudo mkdir -p squashfs_root_bb/procw
sudo mount --bind /dev/urandom /home/mike/NETGEAR_sre/squashfs_root_bb/dev/urandom
sudo mount --bind /dev/random /home/mike/NETGEAR_sre/squashfs_root_bb/dev/random
sudo mount --bind /proc squashfs_root_bb/proc

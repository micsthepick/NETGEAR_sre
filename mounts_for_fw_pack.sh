sudo mkdir -p squashfs_root_bb/proc
sudo mkdir -p squashfs_root_bb/dev/log
sudo mkdir -p squashfs_root_bb/run/systemd/journal
sudo mkdir -p squashfs_root_bb/dev/shm

#sudo touch squashfs_root_bb/dev/tty
sudo touch squashfs_root_bb/dev/urandom squashfs_root_bb/dev/random squashfs_root_bb/dev/log

sudo mount --bind /dev/urandom squashfs_root_bb/dev/urandom
sudo mount --bind /dev/random squashfs_root_bb/dev/random
sudo mount --bind /proc squashfs_root_bb/proc
sudo mount --bind /run/systemd/journal squashfs_root_bb/run/systemd/journal
#sudo mount --bind /dev/tty squashfs_root_bb/dev/tty

# debug FIFO so that we don't ruin command line output
sudo mkfifo squashfs_root_bb/dev/fw_hacks_con

# fake MTD flash
sudo modprobe nandsim first_id_byte=0x20 second_id_byte=0xac third_id_byte=0x00 fourth_id_byte=0x15

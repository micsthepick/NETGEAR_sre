#!/bin/bash
# note for Ubuntu: dash does NOT work
must() {
  "$@"
  local status=$?
  if [ $status -ne 0 ]; then
    echo "Fatal: command failed with status $status: $*"
    exit $status
  fi
}

ensure_char_devnode() {
  if [ -e "$1" ] && [ ! -c "$2" ]; then
    sudo rm -f "$2"
  fi
  if [ ! -c "$2" ]; then
    local maj_hex min_hex maj min
    maj_hex=$(stat -c '%t' "$1")
    min_hex=$(stat -c '%T' "$1")
    maj=$((16#$maj_hex))
    min=$((16#$min_hex))
    must sudo mknod "$2" c $maj $min
  fi
}

ensure_block_devnode() {
  local src="$1" target="$2"
  if [ -e "$target" ] && [ ! -b "$target" ]; then
    sudo rm -f "$target"
  fi
  if [ ! -b "$target" ]; then
    local maj_hex min_hex maj min
    maj_hex=$(stat -c '%t' "$src")
    min_hex=$(stat -c '%T' "$src")
    maj=$((16#$maj_hex))
    min=$((16#$min_hex))
    must sudo mknod "$target" b $maj $min
  fi
}

bind_mount_char_dev() {
  local target="$1$2"
  [ -c "$1" ] || { echo "Not a char device: $1" >&2; return 2; }
  must mkdir -p "$(dirname "$target")"
  ensure_char_devnode "$1" "$target"
  must sudo mount --bind "$1" "$target"
  echo "mounted char dev $target"
}

bind_mount_block_dev() {
  local target="$1$2"
  [ -b "$1" ] || { echo "Not a block device: $1" >&2; return 2; }
  must mkdir -p "$(dirname "$target")"
  ensure_block_devnode "$1" "$target"
  must sudo mount --bind "$1" "$target"
  echo "mounted block dev $target"
}

# Bind mount a directory (creates target dir if needed)
bind_mount_dir() {
  mkdir -p "$2$1"
  echo "mounted dir $2$1"
  sudo mount --bind "$1" "$2$1"
}

# Bind mount a file (creates empty target file if needed)
bind_mount_file() {
  mkdir -p "$(dirname "$2$1")"
  touch "$2$1"
  echo "mounted file $2$1"
  sudo mount --bind "$1" "$2$1"
}

mount_devpts() {
  local target="$2$1"
  sudo mkdir -p "$target"
  sudo mount -t devpts devpts "$target"
  echo "mounted devpts on $target"
}

CHROOT="squashfs_root_bb"

# Create & mount directories
sudo mkdir -p squashfs_root_bb/dev/log
sudo mkdir -p squashfs_root_bb/run/systemd/journal
must bind_mount_dir /proc "$CHROOT"
must bind_mount_dir /run/systemd/journal "$CHROOT"

# Create & mount character devices
must bind_mount_char_dev /dev/tty "$CHROOT"
must bind_mount_char_dev /dev/random "$CHROOT"
must bind_mount_char_dev /dev/urandom "$CHROOT"

# mount devpts
must mount_devpts /dev/pts "$CHROOT"

# Create dev/ptmx device node in chroot if missing
if [ ! -e "$CHROOT/dev/ptmx" ]; then
  sudo chroot "$CHROOT" mknod -m 666 /dev/ptmx c 5 2
fi

./extract_part_table.sh
# fake MTD flash
sudo mkdir -p "$CHROOT/mtd"
sudo cp msmptbl.bin "$CHROOT/mtd/info"
#for i in {0..31}; do
#    sudo touch "$CHROOT/mtd/mtd$i"
#done

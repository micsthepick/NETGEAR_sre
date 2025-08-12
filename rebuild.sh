must() {
  "$@"
  local status=$?
  if [ $status -ne 0 ]; then
    echo "Fatal: command failed with status $status: $*"
    exit $status
  fi
}

be_empty() {
  local dir="$1"
  local contents
  contents=$(ls -A "$dir" 2>/dev/null)
  if [ -n "$contents" ]; then
    echo "Fatal: $dir is not empty, contents:"
    echo "$contents"
    exit 1
  fi
}

CHROOT=squashfs_root_bb

if [ -e "$CHROOT/run/systemd/journal" ]; then
    if mountpoint -q "$CHROOT/run/systemd/journal"; then
        must sudo umount "$CHROOT/run/systemd/journal"
    fi
  must sudo rm -r "$CHROOT/run/systemd/journal"
fi

if [ -e squashfs.tar ]; then
    for fs in dev/random proc dev/urandom dev/tty dev/pts dev/ptmx dev/console dev/log dev/mtdpath dev/ptmx; do
        if [ -e "$CHROOT/$fs" ]; then
            if mountpoint -q "$CHROOT/$fs"; then
                must sudo umount "$CHROOT/$fs"
            fi
            must sudo rm -r "$CHROOT/$fs"
        fi
    done
    must be_empty "$CHROOT/dev"
    must sudo rm -r "$CHROOT"
    must sudo mkdir -p "$CHROOT"
    must sudo tar xf squashfs.tar -C "$CHROOT"
    must sudo ./mounts_for_fw_pack.sh
    must sudo ./first_startup.sh
    must sudo ./cc.sh
else
    echo Create squashfs.tar first!
fi

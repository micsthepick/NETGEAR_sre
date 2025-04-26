find squashfs_root_bb \( -path "squashfs_root_bb/proc" -o -path "squashfs_root_bb/run" \) -prune -o -type f -exec sh -c 'xxd -p "$0" | tr -d "\n" | grep -q "$@" && echo "$0"' {} $@ \;

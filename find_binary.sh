find squashfs_root_bb -type f -exec sh -c 'xxd -p "$0" | tr -d "\n" |  grep -q "$@" && echo "$0"' {} $@ \;

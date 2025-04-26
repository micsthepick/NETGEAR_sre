(cd squashfs_root_bb && grep --color=auto -r --exclude-dir=proc --exclude-dir=run --exclude-dir=dev "$@")

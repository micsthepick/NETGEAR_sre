if [ "$1" = "" ]
then
    echo "please provide a search pattern."
    exit 1
fi
grep --color=auto -r --exclude-dir=proc --exclude-dir=run "$1" squashfs_root_bb $(@:2)

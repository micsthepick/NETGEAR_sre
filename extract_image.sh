[ -e "sqashfs.tar" ] && mv squashfs.tar squashfs.tar.bak
if [ "$1" = "" ]
then
    echo "provide image file"
    exit 1
fi
[ ! -e "_${1##*/}.extracted" ] && binwalk -e $1
([ ! -e "ubifs-root" ] && ~/.local/bin/ubireader_extract_images "_${1##*/}.extracted"/*.ubi) &&
sqfs2tar ./ubifs-root/*.ubi/img-*ubi_rootfs.ubifs > squashfs.tar &&
rm -rf "_${1##*/}.extracted" &&
rm -rf ubifs-root

set -e -o pipefail

rm -f fw_hacks.so
export CFLAGS="-march=armv5te -mfloat-abi=soft -mfpu=vfp"
export MUSLHOME=$HOME/NETGEAR_sre/musl-cross
#make -f Makefile_fw_hacks srcdir=~/NETGEAR_sre/musl-cross/musl-1.2.5 CROSS_COMPILE=$MUSLHOME/bin/arm-linux-musleabi- clean all

#mkdir -p otemp
#(cd otemp && $MUSLHOME/bin/arm-linux-musleabi-ar x ../libcompat_time32.a && rm stat_time32.o lstat_time32.o && $MUSLHOME/bin/arm-linux-musleabi-ar rcs ../libcompat_time32_mod.a *.o)
#rm -r otemp

$MUSLHOME/bin/arm-linux-musleabi-gcc -g $CFLAGS -lc -Werror fw_hacks.c -fPIC -shared -o fw_hacks.so && (sudo cp fw_hacks.so squashfs-root/; sudo cp fw_hacks.so squashfs2/; sudo cp fw_hacks.so squashfs_root_bb) || exit 1
#-L. -Wl,--whole-archive -lcompat_time32_mod -Wl,--no-whole-archive 

if [ ! -f squashfs-root/lib/libc.so.bak ]; then sudo cp squashfs-root/lib/libc.so squashfs-root/lib/libc.so.bak; fi
if [ ! -f squashfs2/lib/libc.so.bak ]; then sudo cp squashfs2/lib/libc.so squashfs2/lib/libc.so.bak; fi

sudo cp $MUSLHOME/arm-linux-musleabi/lib/libc.so squashfs-root/lib/
sudo cp $MUSLHOME/arm-linux-musleabi/lib/libc.so squashfs2/lib/
#sudo ln -s squashfs-root/libc.so squashfs-root/libgcc_s.so.1
#sudo ln -s squashfs2/libc.so squashfs2/libgcc_s.so.1

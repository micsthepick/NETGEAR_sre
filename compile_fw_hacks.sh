rm -f fw_hacks.so
export CFLAGS="-march=armv5te -mfloat-abi=soft -mfpu=vfp"
export FW_HACKS_HOME=$(pwd)
export MUSLHOME=$FW_HACKS_HOME/arm-unknown-linux-musleabi
export gcc="$MUSLHOME/bin/arm-unknown-linux-musleabi-gcc"
$gcc -g $CFLAGS -nostdlib \
  -nodefaultlibs \
  -isystem $MUSLHOME/arm-unknown-linux-musleabi/include \
  -L $MUSLHOME/arm-unknown-linux-musleabi/lib \
  $CFLAGS -Werror ./fw_hacks.c -fPIC -shared -o ./fw_hacks.so

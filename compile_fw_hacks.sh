rm -f fw_hacks.so
CFLAGS="-march=armv5te -mfloat-abi=soft -mfpu=vfp"
FW_HACKS_HOME="$(pwd)"
MUSLHOME="$FW_HACKS_HOME/arm-unknown-linux-musleabi"
gcc="$MUSLHOME/bin/arm-unknown-linux-musleabi-gcc"
$gcc -g $CFLAGS -nostdlib \
  -nodefaultlibs \
  -isystem $MUSLHOME/arm-unknown-linux-musleabi/include \
  -L $MUSLHOME/arm-unknown-linux-musleabi/lib \
  -lpthread \
  $CFLAGS -Werror ./fw_hacks.c -fPIC -shared -o ./fw_hacks.so

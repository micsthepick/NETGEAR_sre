#sudo chmod u+s busybox_install/bin/busybox
mkdir -p ./busybox_install/lib/
cp -rP ./musl-cross/arm-linux-musleabi/lib/* ./busybox_install/lib/

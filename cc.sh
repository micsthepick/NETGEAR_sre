if [ ! -e "./arm-unknown-linux-musleabi" ]
then
    curl -L https://github.com/musl-cross/musl-cross/releases/download/20250206/arm-unknown-linux-musleabi.tar.xz | tar xJ
fi
./compile_fw_hacks.sh && sudo cp ./fw_hacks.so squashfs_root_bb/
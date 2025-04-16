sudo env QEMU_STRACE= chroot ./squashfs-root qemu-arm-static env LD_DEBUG=bindings SHELL=/bin/sh LD_PRELOAD=/fw_hacks.so ash 2>&1 | grep --color=always -P '^(?!.*(mmap|break))|exec|fw_hacks|ld-musl-arm'


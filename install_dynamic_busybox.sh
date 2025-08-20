
#!/bin/bash
. ./common.sh
set -euo pipefail

ORIG_DIR=$(pwd)

TOOLCHAIN_DIR=arm-unknown-linux-musleabi
BUSYBOX_VERSION=1.31.0
BUSYBOX_TARBALL=busybox-${BUSYBOX_VERSION}.tar.bz2
BUSYBOX_SRC_DIR=busybox-${BUSYBOX_VERSION}
TARGET_PATH=squashfs_root_bb/bin/busybox
LIB_ARTIFACT_PATH="$ORIG_DIR/$TOOLCHAIN_DIR/arm-unknown-linux-musleabi/sysroot/usr/lib/"

# Helper function to patch clock_gettime syscall replacement once
patch_clock_gettime_syscall() {
    local file="$1"
    local clock_type="$2"  # e.g. CLOCK_MONOTONIC or CLOCK_REALTIME

    if grep -q "syscall(__NR_clock_gettime, $clock_type, ts)" "$file"; then
        sed -i "s/syscall(__NR_clock_gettime, $clock_type, ts)/clock_gettime($clock_type, ts)/" "$file"
        echo "Patched $file for $clock_type"
    else
        echo "$file already patched for $clock_type"
    fi
}

# Download musl-cross toolchain if missing
if [ ! -d "$ORIG_DIR/$TOOLCHAIN_DIR" ]; then
    echo "Downloading musl-cross toolchain..."
    curl -L https://github.com/musl-cross/musl-cross/releases/download/20250206/${TOOLCHAIN_DIR}.tar.xz | tar xJ -C "$ORIG_DIR"
fi

export CROSS_COMPILE="$ORIG_DIR/${TOOLCHAIN_DIR}/bin/arm-unknown-linux-musleabi-"
export PATH="$ORIG_DIR/${TOOLCHAIN_DIR}/bin:$PATH"

(
    if [ ! -d "$BUSYBOX_SRC_DIR" ]; then
        echo "Downloading BusyBox source..."
        curl -LO https://busybox.net/downloads/${BUSYBOX_TARBALL}
        tar xjf ${BUSYBOX_TARBALL}
    fi

    cd "$BUSYBOX_SRC_DIR"

    # Only rebuild if busybox binary doesn't exist or source/config changed
    if [ ! -f busybox ] || [ busybox -ot .config ] || [ busybox -ot busybox.c ]; then
        # Disable static build only if not disabled already
        if ! grep -q '^# CONFIG_STATIC is not set' .config 2>/dev/null; then
            sed -i 's/CONFIG_STATIC=y/# CONFIG_STATIC is not set/' .config
        fi

        patch_clock_gettime_syscall libbb/time.c CLOCK_MONOTONIC
        patch_clock_gettime_syscall runit/runsv.c CLOCK_REALTIME

        make defconfig > /dev/null
        make ARCH=arm CROSS_COMPILE=${CROSS_COMPILE} CFLAGS="-w" busybox -j$(nproc) > /dev/null
    else
        echo "BusyBox binary is up to date, skipping rebuild."
    fi

    # Copy only if binary differs
    if [ -e "$ORIG_DIR/$TARGET_PATH" ]; then
        if ! cmp -s busybox "$ORIG_DIR/$TARGET_PATH"; then
            echo "Copying updated busybox to $TARGET_PATH with sudo"
            must sudo cp busybox "$ORIG_DIR/$TARGET_PATH"
            echo "Copying libs to $TARGET_PATH with sudo"
            # DO NOT COPY ld-musl-arm from elsewhere, the existing symlink is CORRECT.
            must sudo cp "$LIB_ARTIFACT_PATH/libc.so" "$ORIG_DIR/squashfs_root_bb/lib/"
        else
            echo "Target busybox binary is up to date, skipping copy."
        fi
    else
        echo "Target path $TARGET_PATH does not exist, skipping copy"
    fi
)

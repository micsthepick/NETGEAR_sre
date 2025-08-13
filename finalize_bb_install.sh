#!/bin/bash
. ./common.sh

must mkdir -p ./busybox_install/lib/
must cp -r ./musl-cross/arm-linux-musleabi/lib/* ./busybox_install/lib/

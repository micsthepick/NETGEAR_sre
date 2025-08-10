# netgear router reversing/emulation project

## first steps
loosely following https://boschko.ca/qemu-emulating-firmware/ install qemu
(it's okay if the first command fails, as long as qemu-system, qemu-user-binfmt and qemu-user-static install)
```
sudo apt-get install qemu
sudo apt-get install qemu-user-static
sudo apt-get install qemu-user-binfmt
sudo apt-get install qemu-system
```

use binwalk to extract the squashfs (this article assumes you put the squashfs file in ~/NETGEAR\_sre), and `sudo unsquash` to extract that. Now run:

for me the packaged fw is RBR350-V4.4.2.1.img, yours will most likely be different depending on what you download (try to unzip or untar first where applicable untill you have a .bin or .img)
```
# approximate steps
binwalk -y=ubi <fw>.img
# copy the integer <DECIMAL> offset from above and the other integer "image size: <BYTES> bytes"
dd if=<fw>.img of=ubifs.ubi bs=1 skip=<DECIMAL> count=<BYTES>
# e.g.
#DECIMAL                            HEXADECIMAL                        DESCRIPTION
#-------------------------------------------------------------------------------------------------------------------------------------
#264904                             0x40AC8                            UBI image, version: 1, image size: 43778048 bytes
##dd if=RBR350-V4.4.2.1.img of=ubifs.ubi bs=1 skip=26904 count=43778048
# if the output just gives a starting offset, use that and ignore the count=param, it will just raise a warning when extracting the ubi.
ubireader_extract_images ubifs.ubi
```

Put the arm binary in place
```
sudo cp $(which qemu-arm-static) squashfs-root/$(which qemu-arm-static)
```

use binwalk and python's ubireader to extract the squashfs image,
then use squashfs-tools-ng to create a tarball called squashfs.tar.gz (`sqfs2tar img-*ubi_rootfs.ubifs > ~/NETGEAR_sre/squashfs.tar.gz`)

if at this point you still don't have the capability to chroot, then
ensure `/etc/binfmt.d/` or `/usr/lib/binfmt.d/` contains `qemu-arm.conf`:
```
:qemu-arm:M::\x7fELF\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x28\x00:\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff\xff:./scratch/usr/local/bin/qemu-arm:
```
then try
```
sudo systemctl reload systemd-binfmt
```

now, try running the dev console shell script first (needed to emulate a serial tty)



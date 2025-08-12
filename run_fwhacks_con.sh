TTY="squashfs_root_bb/dev/fw_hacks_con"

while true; do
  if [ ! -c "$TTY" ]; then
    sleep 1
    continue
  fi

  sudo socat -d -d \
    FILE:"$TTY",raw,echo=0 \
    STDIO,raw,echo=0

  # socat exited (device gone or error), retry after 1s
  sleep 1
done

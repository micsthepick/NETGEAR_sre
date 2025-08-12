CONSOLE="squashfs_root_bb/dev/fw_hacks_con"

while true; do
  if [ ! -e "$CONSOLE" ]; then
    echo looking for $CONSOLE
    sleep 1
    continue
  fi

  tail -f "$CONSOLE"
  sleep 1
done

# exec ./run_args.sh /etc/init.d/lighttpd start "2>&1"
exec ./run_args.sh /usr/sbin/lighttpd -D -f /etc/lighttpd/lighttpd.conf "2>&1"


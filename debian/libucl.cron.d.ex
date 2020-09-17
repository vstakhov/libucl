#
# Regular cron jobs for the libucl package
#
0 4	* * *	root	[ -x /usr/bin/libucl_maintenance ] && /usr/bin/libucl_maintenance

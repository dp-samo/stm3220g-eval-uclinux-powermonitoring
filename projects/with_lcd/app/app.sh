#!/bin/sh
# Demo application for the Nokia 6100 LCD

exit 0

fb_init()
{
	insmod spi_a2f_dev.ko spi_a2f_dev_debug=0
}

fb_done()
{
	rmmod spi_a2f_dev
	exit 1
}

# Initialize the framebuffer
fb_init

# We will run an endless loop so prepare to handle traps (^c, etc)
trap fb_done SIGHUP SIGINT SIGTERM

# Visual parameters
bg=-i"bmp/background.bmp"
fg=-fwhite
status_y=118
info_y=12
mem_y=4

# Fill the screen with the background
./lcd_tool -B $bg

# Do the frequent updates
./lcd_tool -x75 -y$mem_y -S"Mem:" -tsmall $fg $bg
while true
do 
	./lcd_tool -x79 -y$status_y -S`date +%H:%M:%S` -tsmall $fg $bg
	sleep 0.5
done &

# Do the less frequent updates
./lcd_tool -x75 -y$mem_y -S"Mem:" -tsmall $fg $bg
while true
do 
	str=2
	./lcd_tool -x18 -y$info_y -S$str -tmedium -fwhite $bg
	str=8
	./lcd_tool -x43 -y$info_y -S$str -tmedium -fwhite $bg
	str=`cat /proc/meminfo | \
		busybox grep MemFree | \
		busybox sed 's/MemFree: //'`
	./lcd_tool -x102 -y$mem_y -S$str -tsmall $fg $bg
	sleep 2
done &

# Do the infrequent updates
while true
do 
	./lcd_tool -x9 -y$status_y -S`date +%b:%d` -tsmall $fg $bg
	sleep 60
done &

# Do the menu 
# Should never quit unless interrupted by a signal
./lcd_menu $bg $fg


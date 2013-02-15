stm3220g-eval-uclinux-powermonitoring
=====================================

uCLinux kernel for stm3220g-eval board, with power-monitoring-oriented logs

Several projects can be found under project/ directoroy but you should first download GNU corss-build from the following link : http://www.codesourcery.com/sgpp/lite/arm/portal/package6503/public/arm-uclinuxeabi/arm-2010q1-189-arm-uclinuxeabi-i686-pc-linux-gnu.tar.bz2 and extract it under tools/ directory.

To generate a linux image suitable for stm3220g-eval board :

$ . ACTIVATE.sh
$ cd projects/wlcdv2
$ make

This will generate wlcdv2.uImage which contains uclinux binary with initramfs image  

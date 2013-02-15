#!/bin/sh
insmod coretftfb.ko coretftfb_debug=4
insmod coretftfb_dev.ko coretftfb_dev_debug=4
rmmod coretftfb_dev
rmmod coretftfb

#! /bin/sh

sudo rmmod sw_dev
make 
sudo insmod sw_dev.ko
sudo mknod /dev/sw_dev0 c 243 0
sudo chmod 666 /dev/sw_dev0

#!/bin/bash

echo "Tests: scd_dev"
echo "-----------------------"

sudo insmod scd_dev.ko

echo "Run basic tests"
sudo ./test
echo "-----------------------"

echo "Show info from /proc (empty)"
cat /proc/scd_dev
echo "-----------------------"

echo "Write some data to scd_dev devices"
echo "-----------------------"
sudo bash -c 'echo "hello my friend" > /dev/scd_dev_0'
sudo bash -c 'echo "bye bye my friend" > /dev/scd_dev_1'
sudo bash -c 'echo "hello again" > /dev/scd_dev_2'

echo "Show info from /proc (filled)"
cat /proc/scd_dev
echo "-----------------------"

# cleanup
sudo bash -c 'read INPUT < /dev/scd_dev_0'
sudo bash -c 'read INPUT < /dev/scd_dev_1'
sudo bash -c 'read INPUT < /dev/scd_dev_2'

sudo rmmod scd_dev.ko


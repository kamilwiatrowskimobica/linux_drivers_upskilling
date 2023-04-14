#!/bin/sh

local0="169.168.0.1" 
remote0="169.168.0.2"
local1="169.168.1.2"
remote1="169.168.1.1"

insmod ./woab_network.ko $*

echo 1 > /proc/sys/net/ipv6/conf/woab0/disable_ipv6
echo 1 > /proc/sys/net/ipv6/conf/woab1/disable_ipv6

ifconfig woab0 $local0 netmask 255.255.255.0
ifconfig woab1 $local1 netmask 255.255.255.0

#sleep 1
ping -c 1 $remote0
#sleep 1
ping -c 1 $remote1
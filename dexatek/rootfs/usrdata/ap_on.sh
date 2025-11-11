#!/bin/sh

touch /var/lib/dhcp/dhcpd.leases

abs_hostapd=/usr/sbin/hostapd
abs_hostapd_conf=/etc/hostapd.conf

ifconfig wlan0 up
ifconfig wlan0 192.168.0.1

sleep 1

$abs_hostapd $abs_hostapd_conf -B

dhcpd -cf /usrdata/dhcpd.conf

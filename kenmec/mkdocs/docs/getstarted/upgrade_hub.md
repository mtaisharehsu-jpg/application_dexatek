# How to upgrade Hub

## Overview

This document describes how to upgrade Hub using OTA image.

## Copy firmware image (.swu) to NFS folder

```bash
cp image.swu /path/to/nfs/folder
```

## Mount to NFS folder

!!! warning "NFS server"
    Please modify 192.168.124.234:/home/xxx/nfs to suit the actual environment.

```bash
mount -t nfs -o nolock,timeo=10 192.168.124.234:/home/kellen/nfs /mnt/
```

## copy image to tmp

```bash
cp /mnt/image.swu /tmp/
```

## Upgrade

```bash
sysupd
```

## Wait for a while for upgrade finished

```bash
[root@Kenmec:/mnt]# sysupd
Update "/tmp/image.swu" to Slot B
Swupdate v2020.04.0

Licensed under GPLv2. See source distribution for detailed copyright notices.

Registered handlers:
	dummy
	flash
	raw
	rawfile
	rawcopy
	shellscript
	preinstall
	postinstall
	ubivol
	ubipartition
	ubiswap
software set: release mode: copy_2
[TRACE] : SWUPDATE running :  [extract_sw_description] : Found file:
	filename sw-description
	size 934
	checksum 0xf002 VERIFIED
[TRACE] : SWUPDATE running :  [listener_create] : creating socket at /tmp/swupdateprog
[TRACE] : SWUPDATE running :  [get_common_fields] : Version 1.0.1
[TRACE] : SWUPDATE running :  [get_common_fields] : Description Augentix system update
[TRACE] : SWUPDATE running :  [parse_images] : Found Image: rootfs.cpio.uboot.bin in device : mtd6 for handler flash
[TRACE] : SWUPDATE running :  [parse_images] : Found Image: uImage.bin in device : mtd5 for handler flash
[TRACE] : SWUPDATE running :  [parse_scripts] : Found Script: post-install.sh
[TRACE] : SWUPDATE running :  [parse_scripts] : Found Script: pre-install.sh
[TRACE] : SWUPDATE running :  [cpio_scan] : Found file:
	filename uImage.bin
	size 1979120
	REQUIRED
[TRACE] : SWUPDATE running :  [network_initializer] : Main loop Daemon
[TRACE] : SWUPDATE running :  [listener_create] : creating socket at /tmp/sockinstctrl
[TRACE] : SWUPDATE running :  [cpio_scan] : Found file:
	filename rootfs.cpio.uboot.bin
	size 29491200
	REQUIRED
[TRACE] : SWUPDATE running :  [cpio_scan] : Found file:
	filename pre-install.sh
	size 507
	REQUIRED
[TRACE] : SWUPDATE running :  [cpio_scan] : Found file:
	filename post-install.sh
	size 491
	REQUIRED
[DEBUG] : SWUPDATE running :  [preupdatecmd] : Running Pre-update command
[TRACE] : SWUPDATE running :  [extract_next_file] : Copied file:
	filename pre-install.sh
	size 507
	checksum 0xa528 VERIFIED
[TRACE] : SWUPDATE running :  [extract_next_file] : Copied file:
	filename post-install.sh
	size 491
	checksum 0xa1a3 VERIFIED
[TRACE] : SWUPDATE running :  [run_system_cmd] : ==== Augentix System Update Framework ====
[TRACE] : SWUPDATE running :  [run_system_cmd] : [SYSUPD] Updating system...
[TRACE] : SWUPDATE running :  [run_system_cmd] : /tmp/scripts/pre-install.sh   command returned 0
[TRACE] : SWUPDATE running :  [install_single_image] : Found installer for stream uImage.bin flash
[TRACE] : SWUPDATE running :  [get_mtd_from_device] : mtd name [mtd5] resolved to [/dev/mtd5]
[TRACE] : SWUPDATE running :  [install_flash_image] : Copying uImage.bin into /dev/mtd5
[TRACE] : SWUPDATE running :  [install_single_image] : Found installer for stream rootfs.cpio.uboot.bin flash
[TRACE] : SWUPDATE running :  [get_mtd_from_device] : mtd name [mtd6] resolved to [/dev/mtd6]
[TRACE] : SWUPDATE running :  [install_flash_image] : Copying rootfs.cpio.uboot.bin into /dev/mtd6
[TRACE] : SWUPDATE running :  [run_system_cmd] : [SYSUPD] System Update Finished. Please reboot the system.
[TRACE] : SWUPDATE running :  [run_system_cmd] : /tmp/scripts/post-install.sh   command returned 0
Software updated successfully
Please reboot the device to start the new software
[INFO ] : SWUPDATE successful ! 
[DEBUG] : SWUPDATE running :  [postupdate] : Running Post-update command
[SYSUPD] New SW version:
1.0.1
[SYSUPD] Update finished; Please reboot system...
```

## Reboot the hub

```bash
reboot
```

## Check the firmware version

```bash

*********************************************
* KENMEC Main Application                    
* Version: 1.0.1                          
* Build Number: 1                           
* Build Date: Sep  5 2025                             
* Build Time: 19:52:28                             
*********************************************


*********************************************
* libdexatek                                 
* Version: 1.0.1                          
* Build Number: 10                           
*********************************************
```
# How to update hub's application

## Prepare

- UART cable and plug in to Hub's log port
- Power on.
- Wait a while for boot finished.

!!! tip "Build the Kenmec Application (Host PC)"
    For detailed build instructions, please refer to [How to build application](build_kenmec.md)

## Kill the Kenmec Application (Hub)

- Get the application's PID (/usr/kenmec/kenmec_main)

```bash
[root@Kenmec:~]# ps
  PID USER       VSZ STAT COMMAND
  ...
  ...
  235 nobody    2124 S    /sbin/mdnsd
  236 root      100m S    /usr/kenmec/kenmec_main
  241 root      1948 S    /usr/sbin/crond
  243 root      1948 S    {autologin.sh} /bin/sh /usr/bin/autologin.sh
  246 root      2052 S    -sh
  ...
```

- Kill the application

```bash
kill -2 236
```

- Or using killall (optional)

```bash
killall kenmec_main
```

## Set the Hub's IP address (optional)

```bash
ifconfig eth0 192.168.124.192 netmask 255.255.255.0
```

!!! warning "IP address"
    Please makesure the IP address must be in same subnet with NFS server. <br>
    If you want to use the default IP address, you can skip this step.

## Mount the NFS folder (Hub)

!!! warning "NFS server"
    Please modify 192.168.124.234:/home/xxx/nfs to suit the actual environment.

```bash
mount -t nfs -o nolock,timeo=10 192.168.124.234:/home/xxx/nfs /mnt/
```

## Copy the application binary to the tmp (Hub)

```bash
cp /mnt/kenmec_main /tmp/
```

## Execute the application (Hub)

```bash
cd /tmp/
./kenmec_main
```

## Replace the application (Hub)

```bash
cp /tmp/kenmec_main /usr/kenmec/kenmec_main
```

!!! warning "Replace the application"
    After replace the application, the new application will launch after reboot.

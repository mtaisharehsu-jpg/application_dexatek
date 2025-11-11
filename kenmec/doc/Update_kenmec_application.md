# Update Kenmec Application Procedure

## Prepare

- UART cable and plug in to Hub's log port
- Power on.
- Wait a while for boot finished.

## (Host PC) Setup the NFS server on your Host PC (example for Ubuntu OS)

Install the NFS server:
```bash
sudo apt install nfs-kernel-server
```

Create the NFS folder:
```bash
mkdir -p /home/xxx/nfs
```

Edit the exports file:
```bash
sudo vim /etc/exports
```

```bash
# /etc/exports: the access control list for filesystems which may be exported
#		to NFS clients.  See exports(5).
#
# Example for NFSv2 and NFSv3:
# /srv/homes       hostname1(rw,sync,no_subtree_check) hostname2(ro,sync,no_subtree_check)
#
# Example for NFSv4:
# /srv/nfs4        gss/krb5i(rw,sync,fsid=0,crossmnt,no_subtree_check)
# /srv/nfs4/homes  gss/krb5i(rw,sync,no_subtree_check)
#
/home/xxx/nfs 192.168.124.0/24(rw,sync,no_subtree_check)
```

Export the NFS folder:
```bash
sudo exportfs -a
```

Restart the NFS server:
```bash
sudo systemctl restart nfs-kernel-server
```

## (Host PC) Build the Kenmec Application

- For detailed build instructions, please refer to [build_kenmec.md](../../../build_kenmec.md)

## (Hub) Kill the Kenmec Application

Get the Kenmec Application (/usr/kenmec/kenmec_main) PID:
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

Kill the Kenmec Application:
```bash
kill -2 236
```

## (Hub) Set the Hub's IP address, please makesure the IP is same network with NFS server

```bash
ifconfig eth0 192.168.124.192 netmask 255.255.255.0
```

## (Hub) Mount the NFS server

```bash
mount -t nfs -o nolock,timeo=10 192.168.124.234:/home/xxx/nfs /mnt/
```

## (Hub) Copy the application binary to the Hub

```bash
cp /mnt/kenmec_main /tmp/
```

## (Hub) Execute the application

```bash
cd /tmp/
./kenmec_main
```
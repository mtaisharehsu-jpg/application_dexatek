# Development tools setup

## NFS server

NFS (Network File System) server allows you to share directories from your host PC with other devices on the same network.

Install:

```bash
sudo apt install nfs-kernel-server
```

Create the NFS folder:

```bash
mkdir -p /path/to/your/nfs/folder
```

Open the exports file:

```bash
sudo vim /etc/exports
```

Add the following line to the exports file:

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
/path/to/your/nfs/folder 192.168.124.0/24(rw,sync,no_subtree_check)
```

!!! warning "Replace the path and IP address"
    Replace `/path/to/your/nfs/folder` with the actual path to the NFS folder. <br>
    Replace `192.168.124.0/24` with the actual IP address of the network.

Export the NFS folder:
```bash
sudo exportfs -a
```

Restart the NFS server:
```bash
sudo systemctl restart nfs-kernel-server
```

---

## cURL (optional)

cURL is a command-line tool for transferring data with URLs. It supports various protocols, including HTTP, HTTPS, FTP, and more. It is widely used for testing APIs, downloading files, and performing network requests.

Install:
```bash
sudo apt update
sudo apt install curl
```

Run:
```bash
curl --version
curl http://www.example.com/
```

---

## Avahi Discovery (optional)

mDNS (Multicast DNS) is a protocol that resolves hostnames to IP addresses within small networks that do not include a local name server. You can use it to discover services and devices on a local network.

Install:
```bash
sudo apt install avahi-daemon
sudo apt install avahi-utils
```

Run:
```bash
avahi-discover
```
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>

#include "cJSON.h"

int get_gateway(const char *ifname, char *gateway) {
    FILE *fp = fopen("/proc/net/route", "r");
    if (!fp) return -1;

    char line[256];
    if(fgets(line, sizeof(line), fp) == NULL)
        return -1;

    while (fgets(line, sizeof(line), fp)) {
        char iface[IFNAMSIZ];
        unsigned long dest, gw;
        int flags, refcnt, use, metric, mask;

        if (sscanf(line, "%s %lx %lx %d %d %d %d %d", iface, &dest, &gw, &flags, &refcnt, &use, &metric, &mask) != 8)
            continue;
        if (strcmp(iface, ifname) == 0 && dest == 0) { 
            struct in_addr gw_addr;
            gw_addr.s_addr = gw;
            strcpy(gateway, inet_ntoa(gw_addr));
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return -1;
}



int get_ipv4_info(const char *ifname, char *ip, char *netmask, char *gateway) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
    
    if (ioctl(sock, SIOCGIFADDR, &ifr) == -1) {
        close(sock);
        return -1;
    }
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
    strcpy(ip, inet_ntoa(sin->sin_addr));

    if (ioctl(sock, SIOCGIFNETMASK, &ifr) == -1) {
        close(sock);
        return -1;
    }
    sin = (struct sockaddr_in *)&ifr.ifr_netmask;
    strcpy(netmask, inet_ntoa(sin->sin_addr));

    close(sock);

    if (get_gateway(ifname, gateway) != 0) {
        strcpy(gateway, "0.0.0.0");
    }

    return 0;
}

int get_mac_address(const char *ifname, char *mac_str) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
        close(sock);
        return -1;
    }

    unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    snprintf(mac_str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    close(sock);
    return 0;
}

int get_speed_mbps(const char *ifname) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct ifreq ifr;
    struct ethtool_cmd edata;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);

    edata.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (caddr_t)&edata;

    if (ioctl(sock, SIOCETHTOOL, &ifr) == -1) {
        close(sock);
        return -1;
    }

    close(sock);

    if (edata.speed == (uint16_t)~0) 
        return -1;

    return edata.speed;
}

int get_interface_status(const char *ifname, int *interface_enabled, char *link_status) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) == -1) {
        close(sock);
        return -1;
    }

    close(sock);

    *interface_enabled = (ifr.ifr_flags & IFF_UP) ? 1 : 0;
    if (ifr.ifr_flags & IFF_RUNNING)
        strcpy(link_status, "LinkUp");
    else
        strcpy(link_status, "LinkDown");

    return 0;
}


bool is_dhcp(const char *ifname) {
    char cmd[128];
    printf("%s : %s\n", __func__, ifname);
    snprintf(cmd, sizeof(cmd), "ip addr show dev %s", ifname);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        perror("popen failed");
        return false;
    }

    char line[512];
    bool is_dhcp_enabled = false;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "inet ") && strstr(line, "dynamic")) {
            is_dhcp_enabled = true;
            break;
        }
    }
    
    pclose(fp);
    return is_dhcp_enabled;
}

int get_ipv6_info(const char *ifname, char *ipv6_addr, char *ipv6_gateway) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip -6 addr show dev %s scope global", ifname);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        perror("popen failed for IPv6 addr");
        return -1;
    }

    char line[512];
    bool found_addr = false;
    
    while (fgets(line, sizeof(line), fp)) {
        // Look for inet6 lines with global scope
        if (strstr(line, "inet6 ") && strstr(line, "scope global")) {
            char *addr_start = strstr(line, "inet6 ") + 6; // Skip "inet6 "
            char *addr_end = strchr(addr_start, '/');
            if (addr_end) {
                int addr_len = addr_end - addr_start;
                if (addr_len < INET6_ADDRSTRLEN) {
                    strncpy(ipv6_addr, addr_start, addr_len);
                    ipv6_addr[addr_len] = '\0';
                    found_addr = true;
                    break;
                }
            }
        }
    }
    
    pclose(fp);
    
    if (!found_addr) {
        strcpy(ipv6_addr, "::");
        return -1;
    }

    // Get IPv6 gateway
    snprintf(cmd, sizeof(cmd), "ip -6 route show dev %s | grep default", ifname);
    fp = popen(cmd, "r");
    if (!fp) {
        strcpy(ipv6_gateway, "::");
        return 0; // We got the address, just no gateway
    }

    if (fgets(line, sizeof(line), fp)) {
        // Parse "default via <gateway> dev <interface>"
        char *via_start = strstr(line, "via ");
        if (via_start) {
            via_start += 4; // Skip "via "
            char *via_end = strchr(via_start, ' ');
            if (via_end) {
                int gw_len = via_end - via_start;
                if (gw_len < INET6_ADDRSTRLEN) {
                    strncpy(ipv6_gateway, via_start, gw_len);
                    ipv6_gateway[gw_len] = '\0';
                } else {
                    strcpy(ipv6_gateway, "::");
                }
            } else {
                strcpy(ipv6_gateway, "::");
            }
        } else {
            strcpy(ipv6_gateway, "::");
        }
    } else {
        strcpy(ipv6_gateway, "::");
    }
    
    pclose(fp);
    return 0;
}

int get_mtu_size(const char *ifname) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);

    if (ioctl(sock, SIOCGIFMTU, &ifr) == -1) {
        close(sock);
        return -1;
    }

    close(sock);
    return ifr.ifr_mtu;
}

int get_ipv6_address_info(const char *ifname, char *address_origin, char *address_state, int *prefix_length) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip -6 addr show dev %s scope global", ifname);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        perror("popen failed for IPv6 addr info");
        return -1;
    }

    char line[512];
    bool found_addr = false;
    
    while (fgets(line, sizeof(line), fp)) {
        // Look for inet6 lines with global scope
        if (strstr(line, "inet6 ") && strstr(line, "scope global")) {
            // Extract prefix length
            char *prefix_start = strchr(line, '/');
            if (prefix_start) {
                prefix_start++; // Skip the '/'
                char *prefix_end = strchr(prefix_start, ' ');
                if (prefix_end) {
                    int prefix_len = prefix_end - prefix_start;
                    char prefix_str[8];
                    if (prefix_len < (int)sizeof(prefix_str)) {
                        strncpy(prefix_str, prefix_start, prefix_len);
                        prefix_str[prefix_len] = '\0';
                        *prefix_length = atoi(prefix_str);
                    } else {
                        *prefix_length = 64; // Default fallback
                    }
                } else {
                    *prefix_length = 64; // Default fallback
                }
            } else {
                *prefix_length = 64; // Default fallback
            }

            // Determine address origin and state based on flags
            if (strstr(line, "dynamic")) {
                strcpy(address_origin, "SLAAC");
                strcpy(address_state, "Preferred");
            } else if (strstr(line, "temporary")) {
                strcpy(address_origin, "SLAAC");
                strcpy(address_state, "Tentative");
            } else if (strstr(line, "deprecated")) {
                strcpy(address_origin, "SLAAC");
                strcpy(address_state, "Deprecated");
            } else {
                // Check if it's a link-local address
                if (strstr(line, "fe80:")) {
                    strcpy(address_origin, "LinkLocal");
                    strcpy(address_state, "Preferred");
                } else {
                    strcpy(address_origin, "Static");
                    strcpy(address_state, "Preferred");
                }
            }
            
            found_addr = true;
            break;
        }
    }
    
    pclose(fp);
    
    if (!found_addr) {
        strcpy(address_origin, "Static");
        strcpy(address_state, "Failed");
        *prefix_length = 64;
        return -1;
    }

    return 0;
}

bool is_eth0_up(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("Failed to create socket for eth0 status check\n");
        return false;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) == -1) {
        printf("Failed to get eth0 flags\n");
        close(sock);
        return false;
    }

    close(sock);
    
    // Check if interface is UP and RUNNING
    bool is_up = (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);
    printf("eth0 status: %s (UP: %s, RUNNING: %s)\n", 
           is_up ? "UP" : "DOWN",
           (ifr.ifr_flags & IFF_UP) ? "YES" : "NO",
           (ifr.ifr_flags & IFF_RUNNING) ? "YES" : "NO");
    
    return is_up;
}

bool is_ipv4_available(void) {
    char ipv4_addr[INET_ADDRSTRLEN] = {0};
    char netmask[INET_ADDRSTRLEN] = {0};
    char gateway[INET_ADDRSTRLEN] = {0};
    
    // Get IPv4 information for eth0
    if (get_ipv4_info("eth0", ipv4_addr, netmask, gateway) != 0) {
        printf("Failed to get IPv4 info for eth0\n");
        return false;
    }
    
    // Check if IP is not 0.0.0.0
    bool has_valid_ip = (strcmp(ipv4_addr, "0.0.0.0") != 0);
    printf("IPv4 status: %s (IP: %s, Netmask: %s, Gateway: %s)\n", 
           has_valid_ip ? "AVAILABLE" : "NOT AVAILABLE",
           ipv4_addr, netmask, gateway);
    
    return has_valid_ip;
}
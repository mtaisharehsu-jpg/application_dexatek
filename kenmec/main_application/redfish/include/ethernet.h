#ifndef ETHERNET_H_
#define ETHERNET_H_

#include <stdbool.h>

int get_speed_mbps(const char *ifname);
int get_gateway(const char *ifname, char *gateway);
int get_mac_address(const char *ifname, char *mac_str);
int get_ipv4_info(const char *ifname, char *ip, char *netmask, char *gateway);
int get_interface_status(const char *ifname, int *interface_enabled, char *link_status);
int get_ipv6_info(const char *ifname, char *ipv6_addr, char *ipv6_gateway);
int get_mtu_size(const char *ifname);
int get_ipv6_address_info(const char *ifname, char *address_origin, char *address_state, int *prefix_length);

bool is_dhcp(const char *ifname);
bool is_eth0_up(void);
bool is_ipv4_available(void);

#endif
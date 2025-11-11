#ifndef  NET_UTILITIES_H
#define  NET_UTILITIES_H

#include <stdint.h>

/*---------------------------------------------------------------------------
								Function Prototypes
---------------------------------------------------------------------------*/

typedef struct {
	uint8_t ipv6:1;
	uint8_t is_dhcp:1;
	uint8_t cable_detected:1;
    uint8_t flag:5;

	uint8_t address[4];
	uint8_t gateway[4];
	uint8_t subnet_mask[4];
} NetUtilitiesEthernetConfig;

int net_ethernet_config_restart(NetUtilitiesEthernetConfig config);

int net_ip_get(const char *ifa_name, unsigned char ip[4]);
int net_subnet_mask_get(const char *ifa_name, unsigned char ip[4]);
int net_default_gateway_get(const char* ifa_name, unsigned char ip[4]);
int net_carrier_detect_get(const char* ifa_name, unsigned char* carrier);
int net_mac_get(const char* ifa_name, unsigned char mac[6]);
int net_mac_dec_string_get(const char* ifa_name, unsigned char mac_str[30], uint16_t* mac_str_len);

int net_config_is_dhcp(unsigned char* is_dhcp);
int net_interface_exist(const char *ifa_name, int *exist);

#endif /* NET_UTILITIES_H */

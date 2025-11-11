#include "dexatek/main_application/include/application_common.h"

#include "dexatek/main_application/include/utilities/net_utilities.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "ethernet.h"
#include "cJSON.h"

static const char *tag = "red_def_json";

void cjson_deep_merge(cJSON *root, cJSON *patch) 
{
    cJSON *patch_child = NULL;
    cJSON_ArrayForEach(patch_child, patch) {
        if (patch_child->string == NULL) continue;
        const char *key = patch_child->string;

        cJSON *root_child = cJSON_GetObjectItem(root, key);
        if (root_child) {
            if (cJSON_IsObject(root_child) && cJSON_IsObject(patch_child)) {
                cjson_deep_merge(root_child, patch_child);
            } else if (cJSON_IsArray(root_child) && cJSON_IsArray(patch_child)) {
                int array_size = cJSON_GetArraySize(patch_child);
                for (int i = 0; i < array_size; i++) {
                    cJSON *patch_item = cJSON_GetArrayItem(patch_child, i);
                    cJSON *root_item = cJSON_GetArrayItem(root_child, i);
                    if (root_item && cJSON_IsObject(root_item) && cJSON_IsObject(patch_item)) {
                        cjson_deep_merge(root_item, patch_item);
                    } else {
                        if (root_item) {
                            cJSON_ReplaceItemInArray(root_child, i, cJSON_Duplicate(patch_item, 1));
                        } else {
                            cJSON_AddItemToArray(root_child, cJSON_Duplicate(patch_item, 1));
                        }
                    }
                }
            } else {
                cJSON_ReplaceItemInObject(root, key, cJSON_Duplicate(patch_child, 1));
            }
        } else {
            cJSON_AddItemToObject(root, key, cJSON_Duplicate(patch_child, 1));
        }
    }
}




void set_default_manager_json(char *source)
{
    snprintf(source, 2048,
        "{\n"
        "    \"@odata.id\": \"/redfish/v1/Managers\",\n"
        "    \"@odata.type\": \"#ManagerCollection.ManagerCollection\",\n"
        "    \"Members\": [\n"
        "        {\n"
        "            \"@odata.id\": \"/redfish/v1/Managers/Kenmec\"\n"
        "        }\n"
        "    ],\n"
        "    \"Members@odata.count\": 1,\n"
        "    \"Name\": \"Manager Collection\"\n"
        "}\n"
    );
}

void set_default_kenmec_json(char *source)
{
    snprintf(source, 2048,
        "{\n"
        "    \"EthernetInterfaces\": {\n"
        "        \"@odata.id\": \"/redfish/v1/Managers/Kenmec/EthernetInterfaces\"\n"
        "    }\n"
        "}\n"
    );
}

void set_default_ethernet_json(char *source)
{
    snprintf(source, 2048,
        "{\n"
        "    \"@odata.id\": \"/redfish/v1/Managers/Kenmec/EthernetInterfaces\",\n"
        "    \"@odata.type\": \"#EthernetInterfaceCollection.EthernetInterfaceCollection\",\n"
        "    \"Description\": \"Collection of EthernetInterfaces for this Manager\",\n"
        "    \"Members\": [\n"
        "        {\n"
        "            \"@odata.id\": \"/redfish/v1/Managers/Kenmec/EthernetInterfaces/eth0\"\n"
        "        }\n"
        "    ],\n"
        "    \"Members@odata.count\": 1,\n"
        "    \"Name\": \"Ethernet Network Interface Collection\"\n"
        "}\n"
    );
}


void set_default_eth0_json(char *source) {
    const char *ifname = "eth0"; 

    char ipv4_addr[INET_ADDRSTRLEN] = {0};
    char netmask[INET_ADDRSTRLEN] = {0};
    char gateway[INET_ADDRSTRLEN] = {0};
    char mac_addr[18] = {0};
    char link_status[16] = {0};
    int interface_enabled = 0;
    int speed_mbps = 0;

    if (get_ipv4_info(ifname, ipv4_addr, netmask, gateway) != 0) {
        strcpy(ipv4_addr, "0.0.0.0");
        strcpy(netmask, "0.0.0.0");
        strcpy(gateway, "0.0.0.0");
    }

    if (get_mac_address(ifname, mac_addr) != 0) {
        strcpy(mac_addr, "00:00:00:00:00:00");
    }

    if (get_interface_status(ifname, &interface_enabled, link_status) != 0) {
        strcpy(link_status, "NoLink");
        interface_enabled = 0;
    }

    speed_mbps = get_speed_mbps(ifname);
    if (speed_mbps < 0) speed_mbps = 0;

    char ipv6_addr[INET6_ADDRSTRLEN] = {0};
    char ipv6_gw[INET6_ADDRSTRLEN] = {0};
    
    if (get_ipv6_info(ifname, ipv6_addr, ipv6_gw) != 0) {
        strcpy(ipv6_addr, "::");
        strcpy(ipv6_gw, "::");
    }

    int mtu_size = get_mtu_size(ifname);
    if (mtu_size < 0) mtu_size = 1500; // Default fallback

    char ipv6_origin[32] = {0};
    char ipv6_state[32] = {0};
    int ipv6_prefix_length = 64;
    
    if (get_ipv6_address_info(ifname, ipv6_origin, ipv6_state, &ipv6_prefix_length) != 0) {
        strcpy(ipv6_origin, "Static");
        strcpy(ipv6_state, "Failed");
        ipv6_prefix_length = 64;
    }

    unsigned char dhcp = 0;
    net_config_is_dhcp(&dhcp);

    info(tag, "ifname: %s, dhcp: %d", ifname, dhcp);

    snprintf(source, 2048,
        "{\n"
        "    \"@odata.id\": \"/redfish/v1/Managers/Kenmec/EthernetInterfaces/eth0\",\n"
        "    \"@odata.type\": \"#EthernetInterface.v1_8_0.EthernetInterface\",\n"
        "    \"FullDuplex\": true,\n"
        "    \"HostName\": \"Kenmec\",\n"
        "    \"IPv4Addresses\": [\n"
        "        {\n"
        "            \"Address\": \"%s\",\n"
        "            \"AddressOrigin\": \"%s\",\n"
        "            \"Gateway\": \"%s\",\n"
        "            \"SubnetMask\": \"%s\"\n"
        "        }\n"
        "    ],\n"
        "    \"IPv6Addresses\": [\n"
        "        {\n"
        "            \"Address\": \"%s\",\n"
        "            \"AddressOrigin\": \"%s\",\n"
        "            \"AddressState\": \"%s\",\n"
        "            \"PrefixLength\": %d\n"
        "        }\n"
        "    ],\n"
        "    \"IPv6DefaultGateway\": \"%s\",\n"
        "    \"IPv6StaticAddresses\": [\n"
        "        {\n"
        "            \"Address\": \"%s\",\n"
        "            \"PrefixLength\": %d\n"
        "        }\n"
        "    ],\n"
        "    \"Id\": \"eth0\",\n"
        "    \"InterfaceEnabled\": %s,\n"
        "    \"LinkStatus\": \"%s\",\n"
        "    \"MACAddress\": \"%s\",\n"
        "    \"MTUSize\": %d,\n"
        "    \"MaxIPv6StaticAddresses\": 1,\n"
        "    \"Name\": \"Manager Ethernet Interface\",\n"
        "    \"NameServers\": [\n"
        "        \"names.dmtf.org\"\n"
        "    ],\n"
        "    \"SpeedMbps\": %d,\n"
        "    \"Status\": {\n"
        "        \"Health\": \"OK\",\n"
        "        \"State\": \"%s\"\n"
        "    }\n"
        "}",
        ipv4_addr,
        dhcp == 1 ? "DHCP" : "Static",
        gateway,
        netmask,
        ipv6_addr,
        ipv6_origin,
        ipv6_state,
        ipv6_prefix_length,
        ipv6_gw,
        ipv6_addr,
        ipv6_prefix_length,
        interface_enabled ? "true" : "false",
        link_status,
        mac_addr,
        mtu_size,
        speed_mbps,
        interface_enabled ? "Enabled" : "Disabled"
    );
}



// void set_default_mobus_table(char *source, int tabale_index)
// {
//     snprintf(source, 2048,
//         "{\n"
//         "    \"@odata.id\": \"/redfish/v1/Managers/Dexatek/EthernetInterfaces\",\n"
//         "    \"@odata.type\": \"#EthernetInterfaceCollection.EthernetInterfaceCollection\",\n"
//         "    \"Description\": \"Collection of EthernetInterfaces for this Manager\",\n"
//         "    \"Members\": [\n"
//         "        {\n"
//         "            \"@odata.id\": \"/redfish/v1/Managers/Dexatek/EthernetInterfaces/eth0\"\n"
//         "        }\n"
//         "    ],\n"
//         "    \"Members@odata.count\": 1,\n"
//         "    \"Name\": \"Ethernet Network Interface Collection\"\n"
//         "}\n"
//     );







// }
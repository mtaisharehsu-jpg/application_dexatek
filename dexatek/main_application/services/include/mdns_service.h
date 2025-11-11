#ifndef MDNS_SERVICE_H
#define MDNS_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dns_sd.h"
#include <arpa/inet.h>

typedef struct {
    DNSServiceRef serviceRef;
    TXTRecordRef txtRecord;

    char name[128];
    char reg_type[128];
    uint16_t port;

    char thing_name[128];

} MdnsServiceConfig;

int mdns_service_txt_update(MdnsServiceConfig* mdns_config);

int mdns_service_run(MdnsServiceConfig* mdns_config);

int mdns_service_stop(MdnsServiceConfig* mdns_config);

#ifdef __cplusplus
}
#endif

#endif
#ifndef SNTP_SERVICE_H
#define SNTP_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <sys/time.h>

#define SNTP_SERVICE_TIMESTAMP_GMP_2024     1704067200

long sntp_service_get_epoch_timestamp(void);
uint64_t sntp_service_get_epoch_timestamp_ms(void);

void sntp_service_init(void);

void sntp_service_stop(void);

#ifdef __cplusplus
}
#endif

#endif
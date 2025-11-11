
#ifndef OS_UTILITIES_H
#define OS_UTILITIES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SECOND (1000)
#define MINUTE (60 * SECOND)
#define HOUR   (60 * MINUTE)
#define DAY    (24 * HOUR)

#ifndef time_after
#define time_after(a,b)	((int64_t)((int64_t)(b) - (int64_t)(a)) < 0)
#endif

#ifndef time_before
#define time_before(a,b)	time_after(b,a)
#endif

int time_delay_ms( const uint32_t xMSToDelay);
int time_delay_noop(uint64_t cnt);

uint64_t time_get_current(void);
uint64_t time_get_current_ms(void);

uint32_t time32_get_current_ms(void);

char* time_get_current_date_string_r(char *buf, size_t sz);

char* time_get_current_date_string_short(char *buf, size_t sz);

uint32_t time_get_uptime_ms(void);

void system_firmware_update(void);
void system_firmware_file_path(char *path);
void system_reboot(void);
void system_reset(void);

#ifdef __cplusplus
}
#endif

#endif

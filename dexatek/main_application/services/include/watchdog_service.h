#ifndef WATCHDOG_SERVICE_H
#define WATCHDOG_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

int watchdog_service_start(int timeout_sec);

int watchdog_service_stop(void);

int watchdog_service__refresh(void);

#ifdef __cplusplus
}
#endif

#endif
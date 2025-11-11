
#ifndef PLATFORM_WATCHDOG_H
#define PLATFORM_WATCHDOG_H

/**
 * @brief This API defines the access of the watchdog. 
 * 
 */

/**
 * @brief Set the timeout in second of the watchdog & start it. The application calls `peripheral_api_watchdog_refresh()` periodically to avoid system reboot.
 * 
 * @return int Return 0 for success, refer to <errno.h> for the error codes.
 */
int platform_watchdog_start(int timeout_sec);

/**
 * @brief Stop the watchdog.
 * 
 * @return int Return 0 for success, refer to <errno.h> for the error codes.
 */
int platform_watchdog_stop(void);

/**
 * @brief Refresh the timeout of the watchdog.
 * 
 * @return int Return 0 for success, refer to <errno.h> for the error codes.
 */
int platform_watchdog_refresh(void);


#endif


#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Hub firmware version */
#define CONFIG_MAJOR_VERSION 1
#define CONFIG_MINOR_VERSION 0
#define CONFIG_PATCH_VERSION 1
#define CONFIG_VERSION_CODE_NUMBER 18

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define CONFIG_WATCHDOG_ENABLE          1
#define CONFIG_WATCHDOG_TIMEOUT_SECONDS 20

#define CONFIG_PLATFORM_MALLOC_DBG      0

/* modbus */
#define CONFIG_MODBUS_DEV       "/dev/ttyAS3"
#define CONFIG_MODBUS_BAUDRATE  115200
#define CONFIG_MODBUS_RE        26
#define CONFIG_MODBUS_DE        40

#define CONFIG_USB_HUB_RESET_PIN 10

#define CONFIG_MODBUS_DATA_FILE_PATH "/usrdata/modbus_data.bin"

#ifdef __cplusplus
}
#endif

#endif

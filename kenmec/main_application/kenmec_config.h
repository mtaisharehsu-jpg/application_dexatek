
#ifndef KENMEC_CONFIG_H
#define KENMEC_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Hub firmware version */
#define CONFIG_APPLICATION_MAJOR_VERSION 1
#define CONFIG_APPLICATION_MINOR_VERSION 3
#define CONFIG_APPLICATION_PATCH_VERSION 1
#define CONFIG_APPLICATION_VERSION_CODE_NUMBER 1

#define CONFIG_APPLICATION_WATCHDOG_ENABLE          0
#define CONFIG_APPLICATION_WATCHDOG_TIMEOUT_SECONDS 20

#define CONFIG_MDNS_ENABLE          1
#define CONFIG_MDNS_NAME            "Kenmec-"
#define CONFIG_MDNS_REG_TYPE        "_redfish._tcp"
#define CONFIG_MDNS_HTTP_PORT       80
#define CONFIG_MDNS_HTTPS_PORT      443
#define CONFIG_REDFISH_VERSION      "1.20.0"

#define CONFIG_APPLICATION_CONTROL_LOGIC_UPDATE_DELAY_MS 1000

#define CONFIG_MODBUS_DEVICE_CONFIG_PATH "/usrdata/modbus_devices_config"

#define CONFIG_TEMPERATURE_CONFIGE_PATH "/usrdata/temperature_configs"

#define CONFIG_ANALOG_INPUT_CURRENT_CONFIGE_PATH "/usrdata/analog_input_current_configs"
#define CONFIG_ANALOG_INPUT_CURRENT_CONFIGE_DEFAULT_PATH "/etc/analog_input_current_configs"

#define CONFIG_ANALOG_INPUT_VOLTAGE_CONFIGE_PATH "/usrdata/analog_input_voltage_configs"

#define CONFIG_ANALOG_OUTPUT_VOLTAGE_CONFIGE_PATH "/usrdata/analog_output_voltage_configs"
#define CONFIG_ANALOG_OUTPUT_CURRENT_CONFIGE_PATH "/usrdata/analog_output_current_configs"

#define CONFIG_SYSTEM_CONFIGS_PATH "/usrdata/system_configs"

#ifndef CONFIG_REDFISH_ACCOUNT_DB_PATH
#define CONFIG_REDFISH_ACCOUNT_DB_PATH "/usrdata/redfish_accounts.db"
#endif

#ifndef CONFIG_REDFISH_TOKEN_VERIFY_ENABLE
#define CONFIG_REDFISH_TOKEN_VERIFY_ENABLE 0
#endif

#ifdef __cplusplus
}
#endif

#endif

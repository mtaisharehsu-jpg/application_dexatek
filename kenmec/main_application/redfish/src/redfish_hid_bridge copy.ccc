#include "redfish_hid_bridge.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <dirent.h>

#include "dexatek/main_application/include/application_common.h"
#include "dexatek/main_application/managers/hid_manager/hid_manager.h"
#include "kenmec/main_application/control_logic/control_logic_manager.h"


static const char* tag = "kenmec_main";

#define DO_0_STR "DO_0"
#define DO_1_STR "DO_1"
#define DO_2_STR "DO_2"
#define DO_3_STR "DO_3"
#define DO_4_STR "DO_4"
#define DO_5_STR "DO_5"
#define DO_6_STR "DO_6"
#define DO_7_STR "DO_7"

#define DI_0_STR "DI_0"
#define DI_1_STR "DI_1"
#define DI_2_STR "DI_2"
#define DI_3_STR "DI_3"
#define DI_4_STR "DI_4"
#define DI_5_STR "DI_5"
#define DI_6_STR "DI_6"
#define DI_7_STR "DI_7"

#define AIO_0_MODE_STR "AIO_0_mode"
#define AIO_1_MODE_STR "AIO_1_mode"
#define AIO_2_MODE_STR "AIO_2_mode"
#define AIO_3_MODE_STR "AIO_3_mode"
#define AIO_0_VOLTAGE_STR "AIO_0_voltage"
#define AIO_1_VOLTAGE_STR "AIO_1_voltage"
#define AIO_2_VOLTAGE_STR "AIO_2_voltage"
#define AIO_3_VOLTAGE_STR "AIO_3_voltage"
#define AIO_0_CURRENT_STR "AIO_0_current"
#define AIO_1_CURRENT_STR "AIO_1_current"
#define AIO_2_CURRENT_STR "AIO_2_current"
#define AIO_3_CURRENT_STR "AIO_3_current"
#define PWM_FREQUENCY_STR "PWM_frequency"
#define PWM_0_FREQ_STR "PWM_0_frequency"
#define PWM_1_FREQ_STR "PWM_1_frequency"
#define PWM_2_FREQ_STR "PWM_2_frequency"
#define PWM_3_FREQ_STR "PWM_3_frequency"
#define PWM_4_FREQ_STR "PWM_4_frequency"
#define PWM_5_FREQ_STR "PWM_5_frequency"
#define PWM_6_FREQ_STR "PWM_6_frequency"
#define PWM_7_FREQ_STR "PWM_7_frequency"
#define PWM_0_PERIOD_STR "PWM_0_period"
#define PWM_1_PERIOD_STR "PWM_1_period"
#define PWM_2_PERIOD_STR "PWM_2_period"
#define PWM_3_PERIOD_STR "PWM_3_period"
#define PWM_4_PERIOD_STR "PWM_4_period"
#define PWM_5_PERIOD_STR "PWM_5_period"
#define PWM_6_PERIOD_STR "PWM_6_period"
#define PWM_7_PERIOD_STR "PWM_7_period"
#define PWM_0_DUTY_STR "PWM_0_duty"
#define PWM_1_DUTY_STR "PWM_1_duty"
#define PWM_2_DUTY_STR "PWM_2_duty"
#define PWM_3_DUTY_STR "PWM_3_duty"
#define PWM_4_DUTY_STR "PWM_4_duty"
#define PWM_5_DUTY_STR "PWM_5_duty"
#define PWM_6_DUTY_STR "PWM_6_duty"
#define PWM_7_DUTY_STR "PWM_7_duty"
#define TEMPERATURE_0_STR "RTD_0_temperature"
#define TEMPERATURE_1_STR "RTD_1_temperature"   
#define TEMPERATURE_2_STR "RTD_2_temperature"
#define TEMPERATURE_3_STR "RTD_3_temperature"
#define TEMPERATURE_4_STR "RTD_4_temperature"
#define TEMPERATURE_5_STR "RTD_5_temperature"
#define TEMPERATURE_6_STR "RTD_6_temperature"
#define TEMPERATURE_7_STR "RTD_7_temperature"

typedef enum  {
    REDFISH_WRITE_NOT_SUPPORTED = 0,
    REDFISH_WRITE_DO_0,
    REDFISH_WRITE_DO_1,
    REDFISH_WRITE_DO_2,
    REDFISH_WRITE_DO_3,
    REDFISH_WRITE_DO_4,
    REDFISH_WRITE_DO_5,
    REDFISH_WRITE_DO_6,
    REDFISH_WRITE_DO_7,
    REDFISH_WRITE_AIO_0_MODE,
    REDFISH_WRITE_AIO_1_MODE,
    REDFISH_WRITE_AIO_2_MODE,
    REDFISH_WRITE_AIO_3_MODE,
    REDFISH_WRITE_AIO_0_VOLTAGE,
    REDFISH_WRITE_AIO_1_VOLTAGE,
    REDFISH_WRITE_AIO_2_VOLTAGE,
    REDFISH_WRITE_AIO_3_VOLTAGE,
    REDFISH_WRITE_AIO_0_CURRENT,
    REDFISH_WRITE_AIO_1_CURRENT,
    REDFISH_WRITE_AIO_2_CURRENT,
    REDFISH_WRITE_AIO_3_CURRENT,
    REDFISH_WRITE_PWM_FREQUENCY,
    REDFISH_WRITE_PWM_0_DUTY,
    REDFISH_WRITE_PWM_1_DUTY,
    REDFISH_WRITE_PWM_2_DUTY,
    REDFISH_WRITE_PWM_3_DUTY,
    REDFISH_WRITE_PWM_4_DUTY,
    REDFISH_WRITE_PWM_5_DUTY,
    REDFISH_WRITE_PWM_6_DUTY,
    REDFISH_WRITE_PWM_7_DUTY,
} REDFISH_WRITE_SUPPORTED_FIELD;

static uint16_t g_hid_pid_list[10];
static size_t g_hid_pid_list_size = 0;
static int g_hid_initialized = 0;

int redfish_hid_init(void) {
    // DUMMY DATA FOR TESTING
    g_hid_pid_list_size = 0;

    for (int port = 0; port < HID_DEVICES_MAX; port++) {
        uint16_t pid = 0;

        // get pid
        hid_manager_port_pid_get(port, &pid);

        // update peripheral data
        switch (pid) {
            case HID_IO_BOARD_PID:
                g_hid_pid_list[port] = pid;
                g_hid_pid_list_size++;
                break;

            case HID_RTD_BOARD_PID:
                g_hid_pid_list[port] = pid;
                g_hid_pid_list_size++;
                break;

            default:
                break;
        }
    }

    g_hid_initialized = 1;

    return SUCCESS;
}

void redfish_hid_deinit(void) {
    g_hid_initialized = 0;
}

int redfish_hid_is_available(void) {
    if (!g_hid_initialized) {
        return 0;
    }
    return 1;
}

int redfish_hid_device_list_get(uint16_t* hid_pid_list_buffer, 
    size_t hid_pid_list_buffer_size, size_t* hid_pid_list_size) {

    if (!g_hid_initialized) {
        return ERROR_NOT_INITIALIZED;
    }

    if (hid_pid_list_buffer_size < g_hid_pid_list_size) {
        return ERROR_INVALID_PARAM;
    }

    memcpy(hid_pid_list_buffer, g_hid_pid_list, g_hid_pid_list_size * sizeof(uint16_t));
    *hid_pid_list_size = g_hid_pid_list_size;

    return SUCCESS;
}

int redfish_hid_open(uint16_t hid_pid) {
    if (!g_hid_initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    (void)hid_pid;
    return SUCCESS;
}

void redfish_hid_close(void) {
    if (!g_hid_initialized) {
        return;
    }
}

// int redfish_hid_write(uint16_t hid_pid, uint16_t hid_port, const uint8_t *write_buffer, size_t write_buffer_size, int timeout_ms) {
//     if (!g_hid_initialized) {
//         return ERROR_NOT_INITIALIZED;
//     }
//     return SUCCESS;
// }

// int redfish_hid_read(uint16_t hid_pid, uint16_t hid_port, uint8_t *buffer, size_t buffer_size, size_t* bytes_read, int timeout_ms) {
//     if (!g_hid_initialized) {
//         return ERROR_NOT_INITIALIZED;
//     }

//     // DUMMY DATA FOR TESTING
//     buffer[0] = 0x01;
//     buffer[1] = 0x02;
//     buffer[2] = 0x03;
//     buffer[3] = 0x04;
//     buffer[4] = 0x05;
//     buffer[5] = 0x06;

//     *bytes_read = 6;

//     return SUCCESS;
// }

static int _get_io_board_write_supported_attribute(cJSON* p_object) 
{
    if (strcmp(DO_0_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_DO_0;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(DO_1_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_DO_1;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(DO_2_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_DO_2;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(DO_3_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_DO_3;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(DO_4_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_DO_4;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(DO_5_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_DO_5;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(DO_6_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_DO_6;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(DO_7_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_DO_7;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_0_MODE_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_0_MODE;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_1_MODE_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_1_MODE;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_2_MODE_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_2_MODE;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_3_MODE_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_3_MODE;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_0_VOLTAGE_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_0_VOLTAGE;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_1_VOLTAGE_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_1_VOLTAGE;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_2_VOLTAGE_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_2_VOLTAGE;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_3_VOLTAGE_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_3_VOLTAGE;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_0_CURRENT_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_0_CURRENT;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_1_CURRENT_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_1_CURRENT;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_2_CURRENT_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_2_CURRENT;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(AIO_3_CURRENT_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_AIO_3_CURRENT;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    }

    return REDFISH_WRITE_NOT_SUPPORTED;
}

static int _get_rtd_board_write_supported_attribute(cJSON* p_object) 
{
    if (strcmp(PWM_FREQUENCY_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_PWM_FREQUENCY;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(PWM_0_DUTY_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_PWM_0_DUTY;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(PWM_1_DUTY_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_PWM_1_DUTY;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(PWM_2_DUTY_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_PWM_2_DUTY;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(PWM_3_DUTY_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_PWM_3_DUTY;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(PWM_4_DUTY_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_PWM_4_DUTY;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(PWM_5_DUTY_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_PWM_2_DUTY;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(PWM_6_DUTY_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_PWM_6_DUTY;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    } else if (strcmp(PWM_7_DUTY_STR, p_object->string) == 0) {
        if (p_object->type == cJSON_Number) {
            return REDFISH_WRITE_PWM_7_DUTY;
        }
        return REDFISH_WRITE_NOT_SUPPORTED;
    }

    return REDFISH_WRITE_NOT_SUPPORTED;
}

int redfish_board_write(uint16_t port_idx, const char *jsonPayload, uint16_t timeout_ms)
{
    int ret = FAIL;

    if (!g_hid_initialized) {
        return ERROR_NOT_INITIALIZED;
    }

    uint16_t port_idx_real = port_idx - 1;

    uint16_t port_pid = 0;
    hid_manager_port_pid_get(port_idx_real, &port_pid);

    // validate
    cJSON* tree = cJSON_Parse(jsonPayload);
    if (tree == NULL) {
        error(tag, "cJSON_Parse = NULL\n");
        return FAIL;
    }

    // json object
    cJSON* jsonObject = tree->child;

    int attribute_value = 0;
    int attribute_type = 0;
    
    // port_pid
    switch (port_pid) {
        case HID_IO_BOARD_PID: {
            // for each field of the request payload
            for ( ; jsonObject != NULL; jsonObject = jsonObject->next) {
                attribute_type = _get_io_board_write_supported_attribute(jsonObject);
                attribute_value = jsonObject->valueint;
                switch (attribute_type) {
                    case REDFISH_WRITE_DO_0:
                        ret = control_hardware_digital_output_set(port_idx_real, 0, attribute_value, timeout_ms);
                        break;
                        
                    case REDFISH_WRITE_DO_1:
                        ret = control_hardware_digital_output_set(port_idx_real, 1, attribute_value, timeout_ms);
                        break;
                        
                    case REDFISH_WRITE_DO_2:
                        ret = control_hardware_digital_output_set(port_idx_real, 2, attribute_value, timeout_ms);
                        break;
                        
                    case REDFISH_WRITE_DO_3:
                        ret = control_hardware_digital_output_set(port_idx_real, 3, attribute_value, timeout_ms);
                        break;
                        
                    case REDFISH_WRITE_DO_4:
                        ret = control_hardware_digital_output_set(port_idx_real, 4, attribute_value, timeout_ms);
                        break;
                        
                    case REDFISH_WRITE_DO_5:
                        ret = control_hardware_digital_output_set(port_idx_real, 5, attribute_value, timeout_ms);
                        break;
                        
                    case REDFISH_WRITE_DO_6:
                        ret = control_hardware_digital_output_set(port_idx_real, 6, attribute_value, timeout_ms);
                        break;
                        
                    case REDFISH_WRITE_DO_7:
                        ret = control_hardware_digital_output_set(port_idx_real, 7, attribute_value, timeout_ms);
                        break;
                        
                    case REDFISH_WRITE_AIO_0_MODE:
                        ret = control_hardware_AI_AO_mode_set(port_idx_real, 0, attribute_value, timeout_ms);
                        break;
                        
                    case REDFISH_WRITE_AIO_1_MODE:
                        ret = control_hardware_AI_AO_mode_set(port_idx_real, 1, attribute_value, timeout_ms);
                        break;

                    case REDFISH_WRITE_AIO_2_MODE:
                        ret = control_hardware_AI_AO_mode_set(port_idx_real, 2, attribute_value, timeout_ms);
                        break;
                        
                    case REDFISH_WRITE_AIO_3_MODE:
                        ret = control_hardware_AI_AO_mode_set(port_idx_real, 3, attribute_value, timeout_ms);
                        break;

                    case REDFISH_WRITE_AIO_0_VOLTAGE:
                        ret = control_hardware_analog_output_voltage_set(port_idx_real, 0, attribute_value, timeout_ms);
                        break;

                    case REDFISH_WRITE_AIO_1_VOLTAGE:
                        ret = control_hardware_analog_output_voltage_set(port_idx_real, 1, attribute_value, timeout_ms);
                        break;

                    case REDFISH_WRITE_AIO_2_VOLTAGE:
                        ret = control_hardware_analog_output_voltage_set(port_idx_real, 2, attribute_value, timeout_ms);
                        break;

                    case REDFISH_WRITE_AIO_3_VOLTAGE:
                        ret = control_hardware_analog_output_voltage_set(port_idx_real, 3, attribute_value, timeout_ms);
                        break;

                    case REDFISH_WRITE_AIO_0_CURRENT:
                        ret = control_hardware_analog_output_current_set(port_idx_real, 0, attribute_value, timeout_ms);
                        break;

                    case REDFISH_WRITE_AIO_1_CURRENT:
                        ret = control_hardware_analog_output_current_set(port_idx_real, 1, attribute_value, timeout_ms);
                        break;

                    case REDFISH_WRITE_AIO_2_CURRENT:
                        ret = control_hardware_analog_output_current_set(port_idx_real, 2, attribute_value, timeout_ms);
                        break;
                        
                    case REDFISH_WRITE_AIO_3_CURRENT:
                        ret = control_hardware_analog_output_current_set(port_idx_real, 3, attribute_value, timeout_ms);
                        break;
                }
            }
            break;
        }

        case HID_RTD_BOARD_PID: {
            // for each field of the request payload
            for ( ; jsonObject != NULL; jsonObject = jsonObject->next) {
                attribute_type = _get_rtd_board_write_supported_attribute(jsonObject);
                attribute_value = jsonObject->valueint;
                switch (attribute_type) {
                    case REDFISH_WRITE_PWM_FREQUENCY:
                        ret = control_hardware_pwm_freq_set(port_idx_real, attribute_value);
                        break;

                    case REDFISH_WRITE_PWM_0_DUTY:
                        attribute_value = jsonObject->valueint;
                        ret = control_hardware_pwm_duty_set(port_idx_real, 0, attribute_value);
                        break;

                    case REDFISH_WRITE_PWM_1_DUTY:
                        attribute_value = jsonObject->valueint;
                        ret = control_hardware_pwm_duty_set(port_idx_real, 1, attribute_value);
                        break;

                    case REDFISH_WRITE_PWM_2_DUTY:
                        attribute_value = jsonObject->valueint;
                        ret = control_hardware_pwm_duty_set(port_idx_real, 2, attribute_value);
                        break;

                    case REDFISH_WRITE_PWM_3_DUTY:
                        attribute_value = jsonObject->valueint;
                        ret = control_hardware_pwm_duty_set(port_idx_real, 3, attribute_value);
                        break;

                    case REDFISH_WRITE_PWM_4_DUTY:
                        attribute_value = jsonObject->valueint;
                        ret = control_hardware_pwm_duty_set(port_idx_real, 4, attribute_value);
                        break;
                        
                    case REDFISH_WRITE_PWM_5_DUTY:
                        attribute_value = jsonObject->valueint;
                        ret = control_hardware_pwm_duty_set(port_idx_real, 5, attribute_value);
                        break;

                    case REDFISH_WRITE_PWM_6_DUTY:
                        attribute_value = jsonObject->valueint;
                        ret = control_hardware_pwm_duty_set(port_idx_real, 6, attribute_value);
                        break;
                        
                    case REDFISH_WRITE_PWM_7_DUTY:
                        attribute_value = jsonObject->valueint;
                        ret = control_hardware_pwm_duty_set(port_idx_real, 7, attribute_value);
                        break;
                }
            }
            break;
        }
    }

    if (ret != SUCCESS) {
        ret = ERROR_INVALID_PARAM;
        error(tag, "redfish_board_write ret = %d", ret);
    }

    return ret;
}

int redfish_board_data_append_to_json(uint16_t port_idx, cJSON *json_root)
{
    if (!g_hid_initialized) {
        return ERROR_NOT_INITIALIZED;
    }
    
    uint16_t port_idx_real = port_idx - 1;

    debug(tag, "[port %d] port_idx_real = %d", port_idx, port_idx_real);

    uint16_t port_pid = 0;
    hid_manager_port_pid_get(port_idx_real, &port_pid);
    
    int32_t temp_val = 0;
    uint16_t val[8] = {0};
    uint32_t val32[8] = {0};
    int32_t s_val32[8] = {0};

    switch (port_pid) {
        case HID_IO_BOARD_PID:
            cJSON_AddNumberToObject(json_root, "Port", port_idx);
            cJSON_AddNumberToObject(json_root, "PID", port_pid);
            // DO
            control_hardware_digital_output_all_get_from_ram(port_idx_real, val);
            cJSON_AddNumberToObject(json_root, DO_0_STR, val[0]);
            cJSON_AddNumberToObject(json_root, DO_1_STR, val[1]);
            cJSON_AddNumberToObject(json_root, DO_2_STR, val[2]);
            cJSON_AddNumberToObject(json_root, DO_3_STR, val[3]);
            cJSON_AddNumberToObject(json_root, DO_4_STR, val[4]);
            cJSON_AddNumberToObject(json_root, DO_5_STR, val[5]);
            cJSON_AddNumberToObject(json_root, DO_6_STR, val[6]);
            cJSON_AddNumberToObject(json_root, DO_7_STR, val[7]);
            // DI
            control_hardware_digital_input_all_get_from_ram(port_idx_real, val);
            cJSON_AddNumberToObject(json_root, DI_0_STR, val[0]);
            cJSON_AddNumberToObject(json_root, DI_1_STR, val[1]);
            cJSON_AddNumberToObject(json_root, DI_2_STR, val[2]);
            cJSON_AddNumberToObject(json_root, DI_3_STR, val[3]);
            cJSON_AddNumberToObject(json_root, DI_4_STR, val[4]);
            cJSON_AddNumberToObject(json_root, DI_5_STR, val[5]);
            cJSON_AddNumberToObject(json_root, DI_6_STR, val[6]);
            cJSON_AddNumberToObject(json_root, DI_7_STR, val[7]);
            // AI mode
            control_hardware_analog_mode_all_get_from_ram(port_idx_real, val);
            cJSON_AddNumberToObject(json_root, AIO_0_MODE_STR, val[0]);
            cJSON_AddNumberToObject(json_root, AIO_1_MODE_STR, val[1]);
            cJSON_AddNumberToObject(json_root, AIO_2_MODE_STR, val[2]);
            cJSON_AddNumberToObject(json_root, AIO_3_MODE_STR, val[3]);
            // AI voltage
            control_hardware_analog_input_voltage_get_from_ram(port_idx_real, 0, &temp_val);
            cJSON_AddNumberToObject(json_root, AIO_0_VOLTAGE_STR, temp_val);
            control_hardware_analog_input_voltage_get_from_ram(port_idx_real, 1, &temp_val);
            cJSON_AddNumberToObject(json_root, AIO_1_VOLTAGE_STR, temp_val);
            control_hardware_analog_input_voltage_get_from_ram(port_idx_real, 2, &temp_val);
            cJSON_AddNumberToObject(json_root, AIO_2_VOLTAGE_STR, temp_val);
            control_hardware_analog_input_voltage_get_from_ram(port_idx_real, 3, &temp_val);
            cJSON_AddNumberToObject(json_root, AIO_3_VOLTAGE_STR, temp_val);
            // AI current
            control_hardware_analog_input_current_get_from_ram(port_idx_real, 0, &temp_val);
            cJSON_AddNumberToObject(json_root, AIO_0_CURRENT_STR, temp_val);
            control_hardware_analog_input_current_get_from_ram(port_idx_real, 1, &temp_val);
            cJSON_AddNumberToObject(json_root, AIO_1_CURRENT_STR, temp_val);
            control_hardware_analog_input_current_get_from_ram(port_idx_real, 2, &temp_val);
            cJSON_AddNumberToObject(json_root, AIO_2_CURRENT_STR, temp_val);
            control_hardware_analog_input_current_get_from_ram(port_idx_real, 3, &temp_val);
            cJSON_AddNumberToObject(json_root, AIO_3_CURRENT_STR, temp_val);
            break;

        case HID_RTD_BOARD_PID:
            cJSON_AddNumberToObject(json_root, "Port", port_idx);
            cJSON_AddNumberToObject(json_root, "PID", port_pid);
            // // PWM duty
            // control_hardware_pwm_duty_all_get_from_ram(port_idx_real, val);
            // cJSON_AddNumberToObject(json_root, PWM_0_DUTY_STR, val[0]);
            // cJSON_AddNumberToObject(json_root, PWM_1_DUTY_STR, val[1]);
            // cJSON_AddNumberToObject(json_root, PWM_2_DUTY_STR, val[2]);
            // cJSON_AddNumberToObject(json_root, PWM_3_DUTY_STR, val[3]);
            // cJSON_AddNumberToObject(json_root, PWM_4_DUTY_STR, val[4]);
            // cJSON_AddNumberToObject(json_root, PWM_5_DUTY_STR, val[5]);
            // cJSON_AddNumberToObject(json_root, PWM_6_DUTY_STR, val[6]);
            // cJSON_AddNumberToObject(json_root, PWM_7_DUTY_STR, val[7]);
            // PWM frequency
            control_hardware_pwm_freq_all_get_from_ram(port_idx_real, val32);
            cJSON_AddNumberToObject(json_root, PWM_0_FREQ_STR, val32[0]);
            cJSON_AddNumberToObject(json_root, PWM_1_FREQ_STR, val32[1]);
            cJSON_AddNumberToObject(json_root, PWM_2_FREQ_STR, val32[2]);
            cJSON_AddNumberToObject(json_root, PWM_3_FREQ_STR, val32[3]);
            cJSON_AddNumberToObject(json_root, PWM_4_FREQ_STR, val32[4]);
            cJSON_AddNumberToObject(json_root, PWM_5_FREQ_STR, val32[5]);
            cJSON_AddNumberToObject(json_root, PWM_6_FREQ_STR, val32[6]);
            cJSON_AddNumberToObject(json_root, PWM_7_FREQ_STR, val32[7]);
            // // PWM period
            // control_hardware_pwm_period_all_get_from_ram(port_idx_real, val32);
            // cJSON_AddNumberToObject(json_root, PWM_0_PERIOD_STR, val32[0]);
            // cJSON_AddNumberToObject(json_root, PWM_1_PERIOD_STR, val32[1]);
            // cJSON_AddNumberToObject(json_root, PWM_2_PERIOD_STR, val32[2]);
            // cJSON_AddNumberToObject(json_root, PWM_3_PERIOD_STR, val32[3]);
            // cJSON_AddNumberToObject(json_root, PWM_4_PERIOD_STR, val32[4]);
            // cJSON_AddNumberToObject(json_root, PWM_5_PERIOD_STR, val32[5]);
            // cJSON_AddNumberToObject(json_root, PWM_6_PERIOD_STR, val32[6]);
            // cJSON_AddNumberToObject(json_root, PWM_7_PERIOD_STR, val32[7]);
            // Temperature
            control_hardware_temperature_all_get_from_ram(port_idx_real, s_val32);
            cJSON_AddNumberToObject(json_root, TEMPERATURE_0_STR, s_val32[0]);
            cJSON_AddNumberToObject(json_root, TEMPERATURE_1_STR, s_val32[1]);
            cJSON_AddNumberToObject(json_root, TEMPERATURE_2_STR, s_val32[2]);
            cJSON_AddNumberToObject(json_root, TEMPERATURE_3_STR, s_val32[3]);
            cJSON_AddNumberToObject(json_root, TEMPERATURE_4_STR, s_val32[4]);
            cJSON_AddNumberToObject(json_root, TEMPERATURE_5_STR, s_val32[5]);
            cJSON_AddNumberToObject(json_root, TEMPERATURE_6_STR, s_val32[6]);
            cJSON_AddNumberToObject(json_root, TEMPERATURE_7_STR, s_val32[7]);
            break;
    }

    return SUCCESS;
}

int redfish_control_logic_data_append_to_json(uint16_t control_logic_idx, cJSON *json_root)
{
    int ret = SUCCESS;

    return control_logic_api_data_append_to_json(control_logic_idx, json_root);
}

int redfish_control_logic_write(uint16_t control_logic_idx, const char *jsonPayload, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    (void)timeout_ms;

    return control_logic_api_write_by_json(control_logic_idx, jsonPayload, timeout_ms);

}
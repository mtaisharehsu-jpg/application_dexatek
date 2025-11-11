/**
 * @file control_logic_common.c
 * @brief 控制邏輯通用功能實現
 *
 * 本文件實現了控制邏輯的通用功能,提供了數據讀寫、格式轉換和API接口。
 *
 * 主要功能:
 * 1. Modbus 寄存器讀寫操作
 * 2. 模擬量輸出值轉換(電壓/電流)
 * 3. 硬體輸出控制(數位輸出、模擬輸出)
 * 4. JSON 數據解析和生成
 * 5. 控制邏輯 API 接口
 *
 * 實現原理:
 * - 通過 Modbus 協議與硬體設備通訊
 * - 支援多種數據類型的轉換(UINT16/INT32/FLOAT32等)
 * - 提供統一的數據讀寫接口給上層應用
 * - 支援 JSON 格式的配置數據讀寫
 *
 * @note 本文件是控制邏輯系統的核心通訊層
 */

#include "dexatek/main_application/include/application_common.h"
#include "dexatek/main_application/managers/modbus_manager/modbus_manager.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"

#include "kenmec/main_application/control_logic/ls80/control_logic_ls80.h"
#include "kenmec/main_application/control_logic/lx1400/control_logic_lx1400.h"

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
/* 日誌標籤 */
static const char* tag = "cl_comm";

/*---------------------------------------------------------------------------
                            Function Prototypes
 ---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
                                Implementation
 ---------------------------------------------------------------------------*/

/**
 * @brief 讀取 Modbus Holding Register
 *
 * 功能說明:
 * 從 Modbus 寄存器表中讀取 16位元無符號整數值。
 *
 * @param address Modbus 寄存器位址
 *                支援三種位址格式:
 *                - 400000+ : 標準 Modbus Holding Register 位址
 *                - 40000+  : 簡化 Modbus Holding Register 位址
 *                - 其他    : 直接使用的內部位址
 * @param value   讀取值的輸出指標
 *
 * @return int 執行結果
 *         - SUCCESS: 讀取成功
 *         - FAIL: 讀取失敗
 *
 * 實現邏輯:
 * 1. 將 Modbus 標準位址轉換為內部位址
 * 2. 從 Modbus 表中加載數據
 */
int control_logic_read_holding_register(uint32_t address, uint16_t *value)
{
    int ret = SUCCESS;

    *value = 0;

    uint32_t target_address = 0;

    /* 將 Modbus 位址轉換為內部目標位址 */
    if (address >= 400000) {
        target_address = address - 400000;
    } else if (address >= 40000) {
        target_address = address - 40000;
    } else {
        target_address = address;
    }

    /* 從 Modbus 表中加載數據 */
    ret = control_logic_load_from_modbus_table(target_address, MODBUS_TYPE_UINT16, value);
    // debug(tag, "addr=%d, target=%d, value=%d", address, target_address, *value);

    return ret;
}

/**
 * @brief 輸出值轉換函數
 *
 * 功能說明:
 * 將用戶輸入的百分比值(0-100%)轉換為硬體實際需要的輸出值。
 * 支援電壓輸出和電流輸出兩種模式。
 *
 * @param port    硬體端口號
 * @param address Modbus 位址(用於判斷輸出類型和通道)
 * @param value   輸入/輸出值指標
 *                輸入: 百分比值(0-100)
 *                輸出: 轉換後的硬體值
 *
 * @return int 執行結果
 *         - SUCCESS: 轉換成功
 *         - FAIL: 轉換失敗(不支援的傳感器類型或位址)
 *
 * 轉換規則:
 * - 電壓輸出: 0-100% -> 0-10000mV (0-10V)
 * - 電流輸出: 0-100% -> 4-20mA
 */
int control_logic_output_value_convert(uint8_t port, uint32_t address, uint16_t *value)
{
    int ret = SUCCESS;

    int channel = -1;

    int value_tmp = *value;

    /* 根據位址獲取通道號 */
    switch (address) {
        case MODBUS_ADDRESS_AD74416H_CH_A_VOLTAGE_OUTPUT_V:
            channel = 0;
            break;
        case MODBUS_ADDRESS_AD74416H_CH_B_VOLTAGE_OUTPUT_V:
            channel = 1;
            break;
        case MODBUS_ADDRESS_AD74416H_CH_C_VOLTAGE_OUTPUT_V:
            channel = 2;
            break;
        case MODBUS_ADDRESS_AD74416H_CH_D_VOLTAGE_OUTPUT_V:
            channel = 3;
            break;
        case MODBUS_ADDRESS_AD74416H_CH_A_CURRENT_OUTPUT:
            channel = 0;
            break;
        case MODBUS_ADDRESS_AD74416H_CH_B_CURRENT_OUTPUT:
            channel = 1;
            break;
        case MODBUS_ADDRESS_AD74416H_CH_C_CURRENT_OUTPUT:
            channel = 2;
            break;
        case MODBUS_ADDRESS_AD74416H_CH_D_CURRENT_OUTPUT:
            channel = 3;
            break;
        default:
            channel = -1;
            break;
    }

    /* 根據不同的輸出類型進行值轉換 */
    switch (address) {
        case MODBUS_ADDRESS_AD74416H_CH_A_VOLTAGE_OUTPUT_V:
        case MODBUS_ADDRESS_AD74416H_CH_B_VOLTAGE_OUTPUT_V:
        case MODBUS_ADDRESS_AD74416H_CH_C_VOLTAGE_OUTPUT_V:
        case MODBUS_ADDRESS_AD74416H_CH_D_VOLTAGE_OUTPUT_V: {
            /* 電壓輸出配置 */
            int config_count = 0;
            analog_config_t *analog_output_voltage_config = control_logic_analog_output_voltage_configs_get(&config_count);
            if (analog_output_voltage_config != NULL && config_count > 0) {
                for (int i = 0; i < config_count; i++) {
                    if (analog_output_voltage_config[i].port == port &&
                        analog_output_voltage_config[i].channel == channel) {
                        switch (analog_output_voltage_config[i].sensor_type) {
                            case 0: /* 0-100% 轉換為 0-10000mV */
                                if (value_tmp < 0) value_tmp = 0;
                                if (value_tmp > 100) value_tmp = 100;
                                *value = value_tmp * 100; /* 1% = 100mV */
                                break;
                            default:
                                ret = FAIL;
                                error(tag, "Not supported sensor type %d", analog_output_voltage_config[i].sensor_type);
                                break;
                        }
                    }
                }
            }
            break;
        }

        case MODBUS_ADDRESS_AD74416H_CH_A_CURRENT_OUTPUT:
        case MODBUS_ADDRESS_AD74416H_CH_B_CURRENT_OUTPUT:
        case MODBUS_ADDRESS_AD74416H_CH_C_CURRENT_OUTPUT:
        case MODBUS_ADDRESS_AD74416H_CH_D_CURRENT_OUTPUT: {
            /* 電流輸出配置 */
            int config_count = 0;
            analog_config_t *analog_output_current_config = control_logic_analog_output_current_configs_get(&config_count);
            if (analog_output_current_config != NULL && config_count > 0) {
                for (int i = 0; i < config_count; i++) {
                    if (analog_output_current_config[i].port == port &&
                        analog_output_current_config[i].channel == channel) {
                        switch (analog_output_current_config[i].sensor_type) {
                            case 0: /* 0-100% 轉換為 4-20mA */
                                /* 線性映射: 0% -> 4mA, 100% -> 20mA */
                                if (value_tmp < 0) value_tmp = 0;
                                if (value_tmp > 100) value_tmp = 100;
                                *value = (uint16_t)(4 + (value_tmp * 16.0f / 100.0f));
                                break;
                            default:
                                ret = FAIL;
                                error(tag, "Not supported sensor type %d", analog_output_current_config[i].sensor_type);
                                break;
                        }
                    }
                }
            }
            break;
        }

        default:
            ret = FAIL;
            error(tag, "Not supported address %d", address);
            break;
    }   

    return ret;
}

/**
 * @brief 寫入控制邏輯寄存器
 *
 * 功能說明:
 * 寫入數據到指定的 Modbus 寄存器,並根據位址類型執行對應的硬體操作。
 * 支援數位輸出、模擬輸出(電壓/電流)等多種硬體控制。
 *
 * @param address    Modbus 寄存器位址
 * @param value      要寫入的值
 * @param timeout_ms 超時時間(毫秒)
 *
 * @return int 執行結果
 *         - SUCCESS: 寫入成功
 *         - FAIL: 寫入失敗
 *
 * 實現邏輯:
 * 1. 將 Modbus 位址轉換為端口和目標位址
 * 2. 對於輸出類型的位址,進行值轉換
 * 3. 根據位址類型執行對應的硬體操作:
 *    - 數位輸出 (GPIO_OUTPUT_0-7)
 *    - 模擬電壓輸出 (AD74416H_VOLTAGE_OUTPUT)
 *    - 模擬電流輸出 (AD74416H_CURRENT_OUTPUT)
 * 4. 對於 Modbus 設備映射的位址,轉發到 RS485 設備
 * 5. 其他位址直接更新到 Modbus 表
 */
int control_logic_write_register(uint32_t address, uint16_t value, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint8_t port = 0;
    int32_t target_address = 0;

    // debug(tag, "address=%d, value=%d", address, value);

    /* 將 Modbus 位址轉換為端口和目標位址 */
    if (address >= 400000) {
        uint32_t address_tmp = address - 400000;
        /* 將位址轉換為端口和基礎位址 */
        if (address_tmp >= HID_BASE_ADDRESS) {
            port = (address_tmp - HID_BASE_ADDRESS) / HID_IO_BOARD_BASE_ADDRESS;
            target_address = (address_tmp - HID_BASE_ADDRESS) % HID_IO_BOARD_BASE_ADDRESS;
        } else if (address_tmp >= HID_BASE_ADDRESS_RTD) {
            port = (address_tmp - HID_BASE_ADDRESS) / HID_RTD_BOARD_BASE_ADDRESS;
            target_address = (address_tmp - HID_BASE_ADDRESS) % HID_RTD_BOARD_BASE_ADDRESS;
        }
        /* 對輸出值進行轉換 */
        switch (target_address) {
            case MODBUS_ADDRESS_AD74416H_CH_A_VOLTAGE_OUTPUT_V:
            case MODBUS_ADDRESS_AD74416H_CH_B_VOLTAGE_OUTPUT_V:
            case MODBUS_ADDRESS_AD74416H_CH_C_VOLTAGE_OUTPUT_V:
            case MODBUS_ADDRESS_AD74416H_CH_D_VOLTAGE_OUTPUT_V:
            case MODBUS_ADDRESS_AD74416H_CH_A_CURRENT_OUTPUT:
            case MODBUS_ADDRESS_AD74416H_CH_B_CURRENT_OUTPUT:
            case MODBUS_ADDRESS_AD74416H_CH_C_CURRENT_OUTPUT:
            case MODBUS_ADDRESS_AD74416H_CH_D_CURRENT_OUTPUT:
                ret = control_logic_output_value_convert(port, target_address, &value);
                if (ret == SUCCESS) {
                    debug(tag, "target_address=%d, new value=%d, ret=%d", target_address, value, ret);
                }
                break;
            default:
                break;
        }

        // output value to hardware
        switch (target_address) {
            case MODBUS_ADDRESS_GPIO_OUTPUT_0:
                ret = control_hardware_digital_output_set(port, 0, value, timeout_ms);
                break;
            case MODBUS_ADDRESS_GPIO_OUTPUT_1:
                ret = control_hardware_digital_output_set(port, 1, value, timeout_ms);
                break;
            case MODBUS_ADDRESS_GPIO_OUTPUT_2:
                ret = control_hardware_digital_output_set(port, 2, value, timeout_ms);
                break;
            case MODBUS_ADDRESS_GPIO_OUTPUT_3:
                ret = control_hardware_digital_output_set(port, 3, value, timeout_ms);
                break;
            case MODBUS_ADDRESS_GPIO_OUTPUT_4:
                ret = control_hardware_digital_output_set(port, 4, value, timeout_ms);
                break;
            case MODBUS_ADDRESS_GPIO_OUTPUT_5:
                ret = control_hardware_digital_output_set(port, 5, value, timeout_ms);
                break;
            case MODBUS_ADDRESS_GPIO_OUTPUT_6:
                ret = control_hardware_digital_output_set(port, 6, value, timeout_ms);
                break;
            case MODBUS_ADDRESS_GPIO_OUTPUT_7:
                ret = control_hardware_digital_output_set(port, 7, value, timeout_ms);
                break;
            case MODBUS_ADDRESS_AD74416H_CH_A_VOLTAGE_OUTPUT_V:
                // mode set first
                ret = control_hardware_AI_AO_mode_set(port, 0, AI_AO_MODE_VOLTAGE_OUT, timeout_ms);
                if (ret == SUCCESS) {
                    ret = control_hardware_analog_output_voltage_set(port, 0, value, timeout_ms);
                }
                break;
            case MODBUS_ADDRESS_AD74416H_CH_B_VOLTAGE_OUTPUT_V:
                // mode set first
                ret = control_hardware_AI_AO_mode_set(port, 1, AI_AO_MODE_VOLTAGE_OUT, timeout_ms);
                if (ret == SUCCESS) {
                    ret = control_hardware_analog_output_voltage_set(port, 1, value, timeout_ms);
                }
                break;
            case MODBUS_ADDRESS_AD74416H_CH_C_VOLTAGE_OUTPUT_V:
                // mode set first
                ret = control_hardware_AI_AO_mode_set(port, 2, AI_AO_MODE_VOLTAGE_OUT, timeout_ms);
                if (ret == SUCCESS) {
                    ret = control_hardware_analog_output_voltage_set(port, 2, value, timeout_ms);
                }
                break;
            case MODBUS_ADDRESS_AD74416H_CH_D_VOLTAGE_OUTPUT_V:
                // mode set first
                ret = control_hardware_AI_AO_mode_set(port, 3, AI_AO_MODE_VOLTAGE_OUT, timeout_ms);
                if (ret == SUCCESS) {
                    ret = control_hardware_analog_output_voltage_set(port, 3, value, timeout_ms);
                }
                break;
    
            case MODBUS_ADDRESS_AD74416H_CH_A_CURRENT_OUTPUT:
                ret = control_hardware_AI_AO_mode_set(port, 0, AI_AO_MODE_CURRENT_OUT, timeout_ms);
                if (ret == SUCCESS) {
                    ret = control_hardware_analog_output_current_set(port, 0, value, timeout_ms);
                }
                break;
                
            case MODBUS_ADDRESS_AD74416H_CH_B_CURRENT_OUTPUT:
                ret = control_hardware_AI_AO_mode_set(port, 1, AI_AO_MODE_CURRENT_OUT, timeout_ms);
                if (ret == SUCCESS) {
                    ret = control_hardware_analog_output_current_set(port, 1, value, timeout_ms);
                }
                break;
                
            case MODBUS_ADDRESS_AD74416H_CH_C_CURRENT_OUTPUT:
                ret = control_hardware_AI_AO_mode_set(port, 2, AI_AO_MODE_CURRENT_OUT, timeout_ms);
                if (ret == SUCCESS) {
                    ret = control_hardware_analog_output_current_set(port, 2, value, timeout_ms);
                }
                break;
                
            case MODBUS_ADDRESS_AD74416H_CH_D_CURRENT_OUTPUT:
                ret = control_hardware_AI_AO_mode_set(port, 3, AI_AO_MODE_CURRENT_OUT, timeout_ms);
                if (ret == SUCCESS) {
                    ret = control_hardware_analog_output_current_set(port, 3, value, timeout_ms);
                }
                break;        
            default:
                break;
        }
        return ret;
    } else if (address >= 40000) {
        target_address = address - 40000;
    } else {
        target_address = address;
    }
    // debug(tag, "port=%d, target_address=%d", port, target_address);

    BOOL modbus_address_mapping_found = FALSE;

    // find modbus address mapping
    int config_count = 0;
    modbus_device_config_t *config = control_logic_modbus_device_configs_get(&config_count);
    if (config != NULL && config_count > 0) {
        for (int i = 0; i < config_count; i++) {
            // debug(tag, "idx %d addr %d code %d upaddr %d", i, config[i].reg_address, config[i].function_code, 
            //                                                   config[i].update_address);
            if (config[i].update_address == target_address && 
                config[i].function_code == MODBUS_FUNC_WRITE_SINGLE_REGISTER) {
                modbus_address_mapping_found = TRUE;
                ret = control_hardware_rs485_single_write(config[i].port, config[i].baudrate, 
                                                          config[i].slave_id, config[i].reg_address, 
                                                          value);
                if (ret == SUCCESS) {
                    ret = control_logic_update_to_modbus_table(target_address, MODBUS_TYPE_UINT16, &value);
                }
                debug(tag, "write to modbus success: address %d, value %d, ret %d", address, value, ret);
                break;
            }
        }
    }
    
    if (modbus_address_mapping_found) {
        debug(tag, "address %d, value %d, bridge to 485 device, ret %d", target_address, value, ret);
    } else {
        ret = control_logic_update_to_modbus_table(target_address, MODBUS_TYPE_UINT16, &value);
        debug(tag, "address %d, value %d, direct update to modbus table, ret %d", target_address, value, ret);
    }

    // debug(tag, "port: %d, address: %d, value: %d, ret = %d", port, target_address, value, ret);

    return ret;
}

/*
{
    "T4": 0,
    "T2": 0,
    "VALVE_OPENING": 0,
    "RegisterConfigs": [
        { "name": "CONTROL_LOGIC_1_ENABLE", "address": 41001, "address_default": 41001}
    ]
}
*/
int control_logic_data_append_to_json(cJSON *json_root, control_logic_register_t *register_list, uint32_t list_size)
{
    int ret = SUCCESS;

    // "RegisterConfigs" array
    cJSON *json_registers_array = cJSON_CreateArray();
    // append all register to json
    for (uint32_t i = 0; i < list_size; i++) {
        // check name
        if (register_list[i].name != NULL && strlen(register_list[i].name) > 0) {
            // json item
            cJSON *json_item = cJSON_CreateObject();
            uint16_t reg_val = 0;
            int32_t reg_address = -1;
            if (register_list[i].address_ptr != NULL) {
                // get address
                reg_address = *(register_list[i].address_ptr);
                // read from modbus register
                control_logic_read_holding_register(reg_address, &reg_val);
            }
            // append to item
            cJSON_AddStringToObject(json_item, "name", register_list[i].name);
            cJSON_AddNumberToObject(json_item, "address", reg_address);
            cJSON_AddNumberToObject(json_item, "address_default", register_list[i].default_address);
            cJSON_AddNumberToObject(json_item, "type", register_list[i].type);
            // append to array
            cJSON_AddItemToArray(json_registers_array, json_item);
            // append register value to root
            cJSON_AddNumberToObject(json_root, register_list[i].name, reg_val);
        }
    }
    cJSON_AddItemToObject(json_root, "RegisterConfigs", json_registers_array);

    return ret;
}

/*
{
    "TEMP_CONTROL_MODE": 0,
}

{
    "RegisterConfigs": [
        {"name": "T4", "address": 413560},
        {"name": "T2", "address": 413556}
    ]
}
*/
int control_logic_write_by_json(const char *jsonPayload, uint16_t timeout_ms, 
                                const char *file_path, control_logic_register_t *register_list, 
                                uint32_t list_size)
{
    int ret = SUCCESS;

    // validate json payload
    cJSON *json_root = cJSON_Parse(jsonPayload);
    if (json_root == NULL) {
        error(tag, "Failed to parse JSON payload");
        return FAIL;
    }

    // "RegisterConfigs" array
    cJSON *registers_array = cJSON_GetObjectItemCaseSensitive(json_root, "RegisterConfigs");
    if (cJSON_IsArray(registers_array)) {
        // to string
        char *registers_array_str = cJSON_PrintUnformatted(registers_array);
        // load register from json
        if (control_logic_register_load_from_json(registers_array_str, register_list, list_size) == SUCCESS) {
            // save to file
            control_logic_register_save_to_file(file_path, registers_array_str);
        }
        // free
        free(registers_array_str);
    }

    // for each register in list
    for (uint32_t i = 0; i < list_size; i++) {
        // check if the register type
        if (register_list[i].type == CONTROL_LOGIC_REGISTER_WRITE ||
            register_list[i].type == CONTROL_LOGIC_REGISTER_READ_WRITE) {
            // check name matche in json
            cJSON *jsonName = cJSON_GetObjectItemCaseSensitive(json_root, register_list[i].name);
            if (cJSON_IsNumber(jsonName)) {
                uint32_t reg_address = 0;
                if (register_list[i].address_ptr != NULL) {
                    reg_address = *(register_list[i].address_ptr);
                }
                // write to modbus register
                ret |= control_logic_write_register(reg_address, 
                                                    (uint16_t)jsonName->valueint, 
                                                    timeout_ms);
                debug(tag, "addr %d, write val %d, ret %d", reg_address, jsonName->valueint, ret);
            }
        }
    }

    // free
    cJSON_Delete(json_root);

    debug(tag, "ret = %d", ret);

    return ret;

}

int control_logic_api_data_append_to_json(uint8_t logic_id, cJSON *json_root)
{
    int ret = SUCCESS;

    control_logic_machine_type_t machine_type = control_logic_config_get_machine_type();
    
    char* file_path = NULL;
    uint32_t list_size = 0;
    control_logic_register_t *register_list = NULL;

    // machine type
    switch (machine_type) {
        case CONTROL_LOGIC_MACHINE_TYPE_LS80: {
            switch (logic_id) {
                case 1:
                    control_logic_ls80_1_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 2:
                    control_logic_ls80_2_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 3:
                    control_logic_ls80_3_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 4:
                    control_logic_ls80_4_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 5:
                    control_logic_ls80_5_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 6:
                    control_logic_ls80_6_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 7:
                    control_logic_ls80_7_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                default:
                    break;
            }
            break;
        }
        case CONTROL_LOGIC_MACHINE_TYPE_LX1400: {
            switch (logic_id) {
                case 1:
                    control_logic_lx1400_1_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 2:
                    control_logic_lx1400_2_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 3:
                    control_logic_lx1400_3_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 4:
                    control_logic_lx1400_4_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 5:
                    control_logic_lx1400_5_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 6:
                    control_logic_lx1400_6_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                case 7:
                    control_logic_lx1400_7_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                    break;
                default:
                    break;
            }
            break;
        }

        default:
            break;
    }
    
    return ret;
}



int control_logic_api_write_by_json(uint8_t logic_id, const char *jsonPayload, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    control_logic_machine_type_t machine_type = control_logic_config_get_machine_type();

    char* file_path = NULL;
    uint32_t list_size = 0;
    control_logic_register_t *register_list = NULL;

    // machine type
    switch (machine_type) {
        case CONTROL_LOGIC_MACHINE_TYPE_LS80: {
            switch (logic_id) {
                case 1:
                    control_logic_ls80_1_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 2:
                    control_logic_ls80_2_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 3:
                    control_logic_ls80_3_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 4:
                    control_logic_ls80_4_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 5:
                    control_logic_ls80_5_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 6:
                    control_logic_ls80_6_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 7:
                    control_logic_ls80_7_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                default:
                    break;
            }
            break;
        }
        case CONTROL_LOGIC_MACHINE_TYPE_LX1400: {
            switch (logic_id) {
                case 1:
                    control_logic_lx1400_1_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 2:
                    control_logic_lx1400_2_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 3:
                    control_logic_lx1400_3_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 4:
                    control_logic_lx1400_4_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 5:
                    control_logic_lx1400_5_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 6:
                    control_logic_lx1400_6_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                case 7:
                    control_logic_lx1400_7_config_get(&list_size, &register_list, &file_path);
                    ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                    break;
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }

    return ret;
}

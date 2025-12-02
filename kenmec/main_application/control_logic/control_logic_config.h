/**
 * @file control_logic_config.h
 * @brief 控制邏輯配置管理頭文件
 *
 * 本文件定義控制邏輯系統的配置資料結構和管理函數。
 * 主要功能包括：
 * - 機型類型定義和管理
 * - Modbus 設備配置
 * - 溫度感測器配置
 * - 類比輸入/輸出配置
 * - 配置檔案的載入和儲存
 */

#ifndef CONTROL_LOGIC_CONFIG_H
#define CONTROL_LOGIC_CONFIG_H

// #include "dexatek/main_application/managers/modbus_manager/modbus_manager.h"

// #include "control_logic_register.h"

/**
 * @brief 控制邏輯機型類型列舉
 *
 * 定義系統支援的機型類型，不同機型使用不同的控制邏輯
 */
typedef enum {
    CONTROL_LOGIC_MACHINE_TYPE_LS80,    /* LS80 機型 */
    CONTROL_LOGIC_MACHINE_TYPE_LX1400,  /* LX1400 機型 */
    CONTROL_LOGIC_MACHINE_TYPE_LS300D,  /* LS300D 機型 */
    CONTROL_LOGIC_MACHINE_TYPE_DEFAULT = CONTROL_LOGIC_MACHINE_TYPE_LS80, /* 預設機型為 LS80 */
} control_logic_machine_type_t;

/**
 * @brief 系統配置結構
 *
 * 存儲系統級別的配置資訊
 */
typedef struct {
    char machine_type[32];      /* 機型類型字串（例如："LS80", "LX1400"） */
} system_config_t;

/**
 * @brief Modbus 設備配置結構
 *
 * 定義透過 RS485 連接的 Modbus 設備的配置參數
 */
typedef struct {
    uint8_t port;               /* USB/HID 埠號索引，用於 RS485 橋接器 */
    uint32_t baudrate;          /* RS485 的 UART 鮑率（例如：9600, 19200, 115200） */
    uint8_t slave_id;           /* Modbus 從站位址（1-247） */
    uint8_t function_code;      /* Modbus 功能碼（例如：03=讀保持暫存器, 04=讀輸入暫存器） */
    uint16_t reg_address;       /* 從站上的起始暫存器/線圈位址 */
    uint8_t data_type;          /* Modbus 資料類型（例如：INT16, UINT16, FLOAT32） */
    float fScale;               /* 縮放因子，用於將原始值轉換為工程單位 */
    int32_t update_address;     /* 本地 Modbus 映射中的目標基址 */
    char name[32];              /* 設備名稱（例如："P1", "F1"） */
} modbus_device_config_t;

/**
 * @brief 溫度感測器配置結構
 *
 * 定義溫度感測器（如 RTD）的配置參數
 */
typedef struct {
    uint8_t port;               /* USB/HID 埠號索引 */
    uint8_t channel;            /* 感測器通道號（0-7） */
    uint8_t sensor_type;        /* 溫度感測器類型（例如：PT100, PT1000） */
    int16_t update_address;     /* 本地 Modbus 映射中的目標基址 */
    char name[32];              /* 感測器名稱（例如："T2", "T11"） */
} temperature_config_t;

/**
 * @brief 類比 I/O 配置結構
 *
 * 定義類比電流/電壓輸入或輸出的配置參數
 */
typedef struct {
    uint8_t port;               /* USB/HID 埠號索引 */
    uint8_t channel;            /* 通道號（0-3） */
    uint8_t sensor_type;        /* 感測器類型（例如：4-20mA, 0-10V） */
    int16_t update_address;     /* 本地 Modbus 映射中的目標基址 */
    char name[32];              /* 名稱（例如："AI1", "AO1"） */
} analog_config_t;

/**
 * @brief 初始化控制邏輯配置
 *
 * 載入並初始化所有控制邏輯的配置資料。
 *
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_config_init(void);

/**
 * @brief 將暫存器配置儲存到檔案
 *
 * 將 JSON 格式的暫存器配置儲存到指定的檔案中。
 *
 * @param file_path 檔案路徑
 * @param jsonPayload JSON 格式的配置資料
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_register_save_to_file(const char *file_path, const char *jsonPayload);

/**
 * @brief 從 JSON 字串載入暫存器配置
 *
 * 解析 JSON 格式的配置字串，載入到暫存器列表中。
 *
 * @param jsonPayload JSON 格式的配置字串
 * @param register_list 暫存器列表指標
 * @param list_size 暫存器列表大小
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_register_load_from_json(const char *jsonPayload, control_logic_register_t *register_list,
                                          uint32_t list_size);

/**
 * @brief 從檔案載入暫存器配置
 *
 * 從指定的檔案讀取並解析暫存器配置。
 *
 * @param file_path 檔案路徑
 * @param register_list 暫存器列表指標
 * @param list_size 暫存器列表大小
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_register_load_from_file(const char *file_path, control_logic_register_t *register_list,
                                          uint32_t list_size);

/* ========== 系統配置函數 ========== */

/**
 * @brief 設定系統配置
 *
 * 從 JSON 字串解析並設定系統配置（如機型類型）。
 *
 * @param json_string JSON 格式的系統配置字串
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_system_configs_set(const char *json_string);

/**
 * @brief 獲取系統配置
 *
 * 返回當前系統配置的指標。
 *
 * @return 指向系統配置結構的指標
 */
system_config_t* control_logic_system_configs_get(void);

/**
 * @brief 獲取機型類型
 *
 * 返回當前配置的機型類型。
 *
 * @return 機型類型列舉值
 */
control_logic_machine_type_t control_logic_config_get_machine_type(void);

/* ========== Modbus 設備配置函數 ========== */

/**
 * @brief 設定 Modbus 設備配置
 *
 * 從 JSON 字串解析並設定 Modbus 設備配置陣列。
 *
 * @param json_string JSON 格式的 Modbus 設備配置字串
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_modbus_device_configs_set(const char *json_string);

/**
 * @brief 獲取 Modbus 設備配置
 *
 * 返回 Modbus 設備配置陣列和設備數量。
 *
 * @param config_count 輸出參數，返回設備配置的數量
 * @return 指向 Modbus 設備配置陣列的指標
 */
modbus_device_config_t* control_logic_modbus_device_configs_get(int *config_count);

/* ========== 溫度感測器配置函數 ========== */

/**
 * @brief 設定溫度感測器配置
 *
 * 從 JSON 字串解析並設定溫度感測器配置陣列。
 *
 * @param json_string JSON 格式的溫度感測器配置字串
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_temperature_configs_set(const char *json_string);

/**
 * @brief 獲取溫度感測器配置
 *
 * 返回溫度感測器配置陣列和感測器數量。
 *
 * @param config_count 輸出參數，返回感測器配置的數量
 * @return 指向溫度感測器配置陣列的指標
 */
temperature_config_t* control_logic_temperature_configs_get(int *config_count);

/* ========== 類比電流輸入配置函數 ========== */

/**
 * @brief 設定類比電流輸入配置
 *
 * 從 JSON 字串解析並設定類比電流輸入配置陣列。
 *
 * @param json_string JSON 格式的類比電流輸入配置字串
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_analog_input_current_configs_set(const char *json_string);

/**
 * @brief 獲取類比電流輸入配置
 *
 * 返回類比電流輸入配置陣列和通道數量。
 *
 * @param config_count 輸出參數，返回配置的數量
 * @return 指向類比電流輸入配置陣列的指標
 */
analog_config_t* control_logic_analog_input_current_configs_get(int *config_count);

/* ========== 類比電壓輸入配置函數 ========== */

/**
 * @brief 設定類比電壓輸入配置
 *
 * 從 JSON 字串解析並設定類比電壓輸入配置陣列。
 *
 * @param json_string JSON 格式的類比電壓輸入配置字串
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_analog_input_voltage_configs_set(const char *json_string);

/**
 * @brief 獲取類比電壓輸入配置
 *
 * 返回類比電壓輸入配置陣列和通道數量。
 *
 * @param config_count 輸出參數，返回配置的數量
 * @return 指向類比電壓輸入配置陣列的指標
 */
analog_config_t* control_logic_analog_input_voltage_configs_get(int *config_count);

/* ========== 類比電壓輸出配置函數 ========== */

/**
 * @brief 設定類比電壓輸出配置
 *
 * 從 JSON 字串解析並設定類比電壓輸出配置陣列。
 *
 * @param json_string JSON 格式的類比電壓輸出配置字串
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_analog_output_voltage_configs_set(const char *json_string);

/**
 * @brief 獲取類比電壓輸出配置
 *
 * 返回類比電壓輸出配置陣列和通道數量。
 *
 * @param config_count 輸出參數，返回配置的數量
 * @return 指向類比電壓輸出配置陣列的指標
 */
analog_config_t* control_logic_analog_output_voltage_configs_get(int *config_count);

/* ========== 類比電流輸出配置函數 ========== */

/**
 * @brief 設定類比電流輸出配置
 *
 * 從 JSON 字串解析並設定類比電流輸出配置陣列。
 *
 * @param json_string JSON 格式的類比電流輸出配置字串
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_analog_output_current_configs_set(const char *json_string);

/**
 * @brief 獲取類比電流輸出配置
 *
 * 返回類比電流輸出配置陣列和通道數量。
 *
 * @param config_count 輸出參數，返回配置的數量
 * @return 指向類比電流輸出配置陣列的指標
 */
analog_config_t* control_logic_analog_output_current_configs_get(int *config_count);

#endif /* CONTROL_LOGIC_MANAGER_H */ 
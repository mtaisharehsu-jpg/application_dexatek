#include "dexatek/main_application/include/application_common.h"

#include "dexatek/main_application/include/utilities/os_utilities.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"

#include "ls80/control_logic_ls80.h"
#include "lx1400/control_logic_lx1400.h"
#include "ls300d/control_logic_ls300d.h"
#include "ls300d/control_logic_ls300d_error.h"
#include "error_handler/error_handler.h"
#include "error_handler/error_history.h"

/**
 * @file control_logic_manager.c
 * @brief 控制邏輯管理器實現
 *
 * 本文件實現了控制邏輯管理器的核心功能,負責管理和調度多個控制邏輯模組。
 *
 * 主要功能:
 * 1. 控制邏輯初始化和配置
 * 2. 多執行緒控制邏輯執行
 * 3. 機器類型適配(LS80/LX1400)
 * 4. 控制邏輯啟動、停止和清理
 *
 * 實現原理:
 * - 使用 CONTROL_LOGIC_ARRAY 存儲多個控制邏輯模組
 * - 每個控制邏輯模組在獨立執行緒中運行
 * - 根據 CONTROL_LOGIC_PROCESS_INTERVAL_MS 定期執行控制邏輯
 * - 支援動態切換不同機器類型的控制函數
 *
 * @note 控制邏輯模組包括:溫度控制、壓力控制、流量控制、泵控制、閥門控制等
 */

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
/* 日誌標籤 */
static const char* tag = "cl_mgr";

/*---------------------------------------------------------------------------
                                Variables
 ---------------------------------------------------------------------------*/
static bool _control_logic_manager_initialized = false;
static bool _control_logic_manager_running = false;

ControlLogic CONTROL_LOGIC_ARRAY[] = {
    {control_logic_ls80_1_temperature_control, NULL, 0, control_logic_ls80_1_temperature_control_init},
    {control_logic_ls80_2_pressure_control, NULL, 0, control_logic_ls80_2_pressure_control_init},
    {control_logic_ls80_3_flow_control, NULL, 0, control_logic_ls80_3_flow_control_init},
    {control_logic_ls80_4_pump_control, NULL, 0, control_logic_ls80_4_pump_control_init},
    {control_logic_ls80_5_waterpump_control, NULL, 0, control_logic_ls80_5_waterpump_control_init},
    {control_logic_ls80_6_valve_control, NULL, 0, control_logic_ls80_6_valve_control_init}, 
    {control_logic_ls80_7_2dc_pump_control, NULL, 0, control_logic_ls80_7_2dc_pump_control_init}
};

/*---------------------------------------------------------------------------
                            Function Prototypes
 ---------------------------------------------------------------------------*/
/**
 * @brief 控制邏輯管理器執行緒函數
 * @param arg 執行緒參數(控制邏輯陣列索引)
 * @return NULL
 */
static void* _control_logic_manager_thread_func(void* arg);

/*---------------------------------------------------------------------------
                                Implementation
 ---------------------------------------------------------------------------*/

/**
 * @brief 根據機器類型設置控制邏輯函數指標
 *
 * 功能說明:
 * 根據不同的機器類型(LS80或LX1400),將對應的控制邏輯函數和初始化函數
 * 指標設置到 CONTROL_LOGIC_ARRAY 中。
 *
 * @param machine_type 機器類型
 *                     - CONTROL_LOGIC_MACHINE_TYPE_LS80: LS80機型
 *                     - CONTROL_LOGIC_MACHINE_TYPE_LX1400: LX1400機型
 *
 * @return int 執行結果
 *         - SUCCESS: 設置成功
 *
 * 實現邏輯:
 * 1. 根據機器類型選擇對應的控制邏輯函數
 * 2. 設置7個控制邏輯模組的執行函數(func)
 * 3. 設置7個控制邏輯模組的初始化函數(init)
 */
int control_logic_manager_set_function_pointer(int machine_type)
{
    int ret = SUCCESS;

    switch (machine_type) {
        default:
        case CONTROL_LOGIC_MACHINE_TYPE_LS80:
            ret = SUCCESS;
            /* 設置 LS80 機型的控制函數 */
            CONTROL_LOGIC_ARRAY[0].func = control_logic_ls80_1_temperature_control;
            CONTROL_LOGIC_ARRAY[1].func = control_logic_ls80_2_pressure_control;
            CONTROL_LOGIC_ARRAY[2].func = control_logic_ls80_3_flow_control;
            CONTROL_LOGIC_ARRAY[3].func = control_logic_ls80_4_pump_control;
            CONTROL_LOGIC_ARRAY[4].func = control_logic_ls80_5_waterpump_control;
            CONTROL_LOGIC_ARRAY[5].func = control_logic_ls80_6_valve_control;
            CONTROL_LOGIC_ARRAY[6].func = control_logic_ls80_7_2dc_pump_control;
            /* 設置 LS80 機型的初始化函數 */
            CONTROL_LOGIC_ARRAY[0].init = control_logic_ls80_1_temperature_control_init;
            CONTROL_LOGIC_ARRAY[1].init = control_logic_ls80_2_pressure_control_init;
            CONTROL_LOGIC_ARRAY[2].init = control_logic_ls80_3_flow_control_init;
            CONTROL_LOGIC_ARRAY[3].init = control_logic_ls80_4_pump_control_init;
            CONTROL_LOGIC_ARRAY[4].init = control_logic_ls80_5_waterpump_control_init;
            CONTROL_LOGIC_ARRAY[5].init = control_logic_ls80_6_valve_control_init;
            CONTROL_LOGIC_ARRAY[6].init = control_logic_ls80_7_2dc_pump_control_init;
            break;

        case CONTROL_LOGIC_MACHINE_TYPE_LX1400:
            ret = SUCCESS;
            /* 設置 LX1400 機型的控制函數 */
            CONTROL_LOGIC_ARRAY[0].func = control_logic_lx1400_1_temperature_control;
            CONTROL_LOGIC_ARRAY[1].func = control_logic_lx1400_2_pressure_control;
            CONTROL_LOGIC_ARRAY[2].func = control_logic_lx1400_3_flow_control;
            CONTROL_LOGIC_ARRAY[3].func = control_logic_lx1400_4_pump_control;
            CONTROL_LOGIC_ARRAY[4].func = control_logic_lx1400_5_waterpump_control;
            CONTROL_LOGIC_ARRAY[5].func = control_logic_lx1400_6_valve_control;
            CONTROL_LOGIC_ARRAY[6].func = control_logic_lx1400_7_2dc_pump_control;
            /* 設置 LX1400 機型的初始化函數 */
            CONTROL_LOGIC_ARRAY[0].init = control_logic_lx1400_1_temperature_control_init;
            CONTROL_LOGIC_ARRAY[1].init = control_logic_lx1400_2_pressure_control_init;
            CONTROL_LOGIC_ARRAY[2].init = control_logic_lx1400_3_flow_control_init;
            CONTROL_LOGIC_ARRAY[3].init = control_logic_lx1400_4_pump_control_init;
            CONTROL_LOGIC_ARRAY[4].init = control_logic_lx1400_5_waterpump_control_init;
            CONTROL_LOGIC_ARRAY[5].init = control_logic_lx1400_6_valve_control_init;
            CONTROL_LOGIC_ARRAY[6].init = control_logic_lx1400_7_2dc_pump_control_init;
            break;

        case CONTROL_LOGIC_MACHINE_TYPE_LS300D:
            ret = SUCCESS;
            /* 設置 LS300D 機型的控制函數 */
            CONTROL_LOGIC_ARRAY[0].func = control_logic_ls300d_1_temperature_control;
            CONTROL_LOGIC_ARRAY[1].func = control_logic_ls300d_2_pressure_control;
            CONTROL_LOGIC_ARRAY[2].func = control_logic_ls300d_3_flow_control;
            CONTROL_LOGIC_ARRAY[3].func = control_logic_ls300d_4_pump_control;
            CONTROL_LOGIC_ARRAY[4].func = control_logic_ls300d_5_waterpump_control;
            CONTROL_LOGIC_ARRAY[5].func = control_logic_ls300d_6_valve_control;
            CONTROL_LOGIC_ARRAY[6].func = control_logic_ls300d_7_2dc_pump_control;
            /* 設置 LS300D 機型的初始化函數 */
            CONTROL_LOGIC_ARRAY[0].init = control_logic_ls300d_1_temperature_control_init;
            CONTROL_LOGIC_ARRAY[1].init = control_logic_ls300d_2_pressure_control_init;
            CONTROL_LOGIC_ARRAY[2].init = control_logic_ls300d_3_flow_control_init;
            CONTROL_LOGIC_ARRAY[3].init = control_logic_ls300d_4_pump_control_init;
            CONTROL_LOGIC_ARRAY[4].init = control_logic_ls300d_5_waterpump_control_init;
            CONTROL_LOGIC_ARRAY[5].init = control_logic_ls300d_6_valve_control_init;
            CONTROL_LOGIC_ARRAY[6].init = control_logic_ls300d_7_2dc_pump_control_init;
            break;
    }

    return ret;
}

/**
 * @brief 控制邏輯管理器執行緒函數
 *
 * 功能說明:
 * 每個控制邏輯模組在獨立執行緒中運行,定期執行控制邏輯。
 *
 * @param arg 執行緒參數,為控制邏輯陣列的索引值
 * @return NULL
 *
 * 實現邏輯:
 * 1. 獲取當前時間戳
 * 2. 首次執行時初始化時間戳
 * 3. 檢查是否達到執行間隔時間(CONTROL_LOGIC_PROCESS_INTERVAL_MS)
 * 4. 如果達到間隔時間,執行對應的控制邏輯函數
 * 5. 更新最後執行時間戳
 * 6. 如果未達到間隔時間,延遲剩餘時間後繼續
 *
 * @note 執行緒會持續運行,不會退出
 */
static void* _control_logic_manager_thread_func(void* arg)
{
    /* 獲取控制邏輯陣列索引 */
    int index = (int)arg;

    while (1) {
        /* 獲取當前時間戳 */
        uint32_t current_timestamp_ms = time32_get_current_ms();

        /* 首次執行時初始化時間戳 */
        if(CONTROL_LOGIC_ARRAY[index].latest_timestamp_ms == 0  ) {
            CONTROL_LOGIC_ARRAY[index].latest_timestamp_ms = current_timestamp_ms;
            continue;
        }

        /* 檢查是否達到執行間隔,如果達到則執行控制邏輯 */
        if(current_timestamp_ms - CONTROL_LOGIC_ARRAY[index].latest_timestamp_ms >= CONTROL_LOGIC_PROCESS_INTERVAL_MS) {
            if (CONTROL_LOGIC_ARRAY[index].func != NULL) {
                /* 執行控制邏輯函數 */
                CONTROL_LOGIC_ARRAY[index].func(&CONTROL_LOGIC_ARRAY[index]);
            }
            /* 更新最後執行時間戳 */
            CONTROL_LOGIC_ARRAY[index].latest_timestamp_ms = current_timestamp_ms;
        }
        else {
            /* 延遲剩餘時間 */
            time_delay_ms(CONTROL_LOGIC_PROCESS_INTERVAL_MS - (current_timestamp_ms - CONTROL_LOGIC_ARRAY[index].latest_timestamp_ms));
        }
    }
    return NULL;
}

/**
 * @brief 重新初始化控制邏輯管理器
 *
 * 功能說明:
 * 在運行時重新初始化控制邏輯管理器,通常用於機器類型變更後的重新配置。
 *
 * @return int 執行結果
 *         - SUCCESS: 初始化成功
 *         - 其他值: 初始化失敗
 *
 * 實現邏輯:
 * 1. 根據當前機器類型設置函數指標
 * 2. 初始化硬體設備
 * 3. 執行所有控制邏輯模組的初始化函數
 */
int control_logic_manager_reinit(void)
{
    int ret = SUCCESS;

    /* 根據機器類型設置函數指標 */
    control_logic_manager_set_function_pointer(control_logic_config_get_machine_type());

    /* 初始化硬體設備 */
    ret = control_hardware_init(control_logic_config_get_machine_type());
    if (ret != SUCCESS) {
        error(tag, "Failed to initialize control logic hardware");
        return ret;
    }

    /* 初始化所有控制邏輯模組 */
    for (size_t i = 0; i < sizeof(CONTROL_LOGIC_ARRAY) / sizeof(CONTROL_LOGIC_ARRAY[0]); i++) {
        if (CONTROL_LOGIC_ARRAY[i].init != NULL) {
            CONTROL_LOGIC_ARRAY[i].init();
        }
    }

    return ret;
}

/**
 * @brief 初始化控制邏輯管理器
 *
 * 功能說明:
 * 初始化控制邏輯管理器,包括硬體初始化、控制邏輯初始化和執行緒創建。
 *
 * @return int 執行結果
 *         - 0: 初始化成功
 *         - -1: 初始化失敗
 *
 * 實現邏輯:
 * 1. 檢查是否已經初始化,避免重複初始化
 * 2. 初始化硬體設備
 * 3. 根據機器類型設置函數指標
 * 4. 執行所有控制邏輯模組的初始化函數
 * 5. 為每個控制邏輯模組創建獨立執行緒
 * 6. 設置初始化完成標誌
 */
int control_logic_manager_init(void)
{

    debug(tag, "VVVV222111***********************999111");

    /* 檢查是否已經初始化 */
    if (_control_logic_manager_initialized) {
        debug(tag, "Control logic already initialized");
        return 0;
    }

    debug(tag, "Initializing control logic hardware...");

    /* 初始化硬體設備 */
    control_hardware_init(control_logic_config_get_machine_type());

    debug(tag, "Initializing control logic...");

    /* 根據機器類型設置函數指標 */
    control_logic_manager_set_function_pointer(control_logic_config_get_machine_type());

    /* 初始化所有控制邏輯模組 */
    for (size_t i = 0; i < sizeof(CONTROL_LOGIC_ARRAY) / sizeof(CONTROL_LOGIC_ARRAY[0]); i++) {
        if (CONTROL_LOGIC_ARRAY[i].init != NULL) {
            CONTROL_LOGIC_ARRAY[i].init();
        }
    }

    /* 初始化錯誤處理模組 */
    debug(tag, "Initializing error handler...");
    if (error_handler_init() != 0) {
        error(tag, "Failed to initialize error handler");
        return -1;
    }

    /* 根據機型初始化對應的錯誤碼 */
    int machine_type = control_logic_config_get_machine_type();
    if (machine_type == CONTROL_LOGIC_MACHINE_TYPE_LS300D) {
        debug(tag, "Initializing LS300D error codes...");
        if (control_logic_ls300d_error_init() != 0) {
            error(tag, "Failed to initialize LS300D error codes");
            /* 錯誤碼初始化失敗不影響主程式運行 */
        }
    }
    /* TODO: 未來可在此處添加其他機型的錯誤碼初始化 */

    /* 設置初始化完成標誌 */
    _control_logic_manager_initialized = true;

    /* 為每個控制邏輯模組創建執行緒 */
    for (size_t i = 0; i < sizeof(CONTROL_LOGIC_ARRAY) / sizeof(CONTROL_LOGIC_ARRAY[0]); i++) {
        if (pthread_create(&CONTROL_LOGIC_ARRAY[i].thread_handle, NULL, _control_logic_manager_thread_func, (void*)i) != 0) {
            error(tag, "Failed to create control logic thread");
            return -1;
        }
    }

    debug(tag, "Control logic initialized successfully");
    return 0;
}

/**
 * @brief 啟動控制邏輯管理器
 *
 * 功能說明:
 * 啟動控制邏輯管理器,使其開始執行控制邏輯。
 *
 * @return int 執行結果
 *         - 0: 啟動成功
 *         - -1: 啟動失敗(未初始化)
 *
 * @note 需要先調用 control_logic_manager_init() 進行初始化
 */
int control_logic_manager_start(void)
{
    /* 檢查是否已初始化 */
    if (!_control_logic_manager_initialized) {
        error(tag, "Control logic not initialized");
        return -1;
    }

    /* 檢查是否已在運行中 */
    if (_control_logic_manager_running) {
        debug(tag, "Control logic already running");
        return 0;
    }

    debug(tag, "Starting control logic...");

    /* 設置運行狀態標誌 */
    _control_logic_manager_running = true;
    debug(tag, "Control logic started successfully");
    return 0;
}

/**
 * @brief 停止控制邏輯管理器
 *
 * 功能說明:
 * 停止控制邏輯管理器的執行。
 *
 * @return int 執行結果
 *         - 0: 停止成功或已停止
 */
int control_logic_manager_stop(void)
{
    /* 檢查是否在運行中 */
    if (!_control_logic_manager_running) {
        debug(tag, "Control logic not running");
        return 0;
    }

    debug(tag, "Stopping control logic...");

    /* 清除運行狀態標誌 */
    _control_logic_manager_running = false;
    debug(tag, "Control logic stopped successfully");
    return 0;
}

/**
 * @brief 清理控制邏輯管理器
 *
 * 功能說明:
 * 停止並清理控制邏輯管理器,釋放相關資源。
 *
 * 實現邏輯:
 * 1. 如果正在運行,先停止
 * 2. 清除初始化狀態標誌
 */
void control_logic_manager_cleanup(void)
{
    /* 如果正在運行,先停止 */
    if (_control_logic_manager_running) {
        control_logic_manager_stop();
    }

    debug(tag, "Cleaning up control logic...");

    /* 清理錯誤處理模組 */
    error_handler_cleanup();

    /* 清除初始化狀態標誌 */
    _control_logic_manager_initialized = false;

    debug(tag, "Control logic cleanup completed");
}

/**
 * @brief 檢查控制邏輯管理器是否在運行
 *
 * @return bool 運行狀態
 *         - true: 正在運行
 *         - false: 已停止
 */
bool control_logic_manager_is_running(void)
{
    return _control_logic_manager_running;
}

/**
 * @brief 獲取控制邏輯模組數量
 *
 * @return int 控制邏輯模組數量
 */
int control_logic_manager_number_of_control_logics(void) {
    return sizeof(CONTROL_LOGIC_ARRAY) / sizeof(CONTROL_LOGIC_ARRAY[0]);
}

/*---------------------------------------------------------------------------
                            Error Handler API Implementation
 ---------------------------------------------------------------------------*/

/**
 * @brief 獲取錯誤碼活動數量
 */
uint32_t control_logic_manager_get_active_error_count(void)
{
    return error_handler_get_active_count();
}

/**
 * @brief 獲取錯誤碼狀態 JSON
 */
int control_logic_manager_get_errors_json(cJSON *json_root)
{
    if (json_root == NULL) {
        return -1;
    }
    return error_handler_to_json(json_root);
}

/**
 * @brief 獲取活動錯誤碼 JSON
 */
int control_logic_manager_get_active_errors_json(cJSON *json_root)
{
    if (json_root == NULL) {
        return -1;
    }
    return error_handler_active_to_json(json_root);
}

/**
 * @brief 確認錯誤碼
 */
int control_logic_manager_acknowledge_error(uint32_t error_code)
{
    return error_handler_acknowledge(error_code);
}

/**
 * @brief 確認所有活動錯誤碼
 */
int control_logic_manager_acknowledge_all_errors(void)
{
    return error_handler_acknowledge_all();
}

/**
 * @brief 重置錯誤碼
 */
int control_logic_manager_reset_error(uint32_t error_code)
{
    return error_handler_reset(error_code);
}

/**
 * @brief 獲取錯誤歷史記錄 JSON
 */
int control_logic_manager_get_error_history_json(uint32_t count, cJSON *json_root)
{
    if (json_root == NULL) {
        return -1;
    }
    return error_history_get_recent(count, json_root);
}

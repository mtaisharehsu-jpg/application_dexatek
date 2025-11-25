/*
 * control_logic_ls80_5.c - LS80 補水泵控制邏輯 (Control Logic 5: Water Pump Control)
 *
 * 【功能概述】
 * 本模組實現 CDU 系統的補水泵控制功能,根據水箱液位自動補水,維持系統水位穩定。
 * 支援手動/自動模式,並提供完整的液位監控、安全保護和故障處理機制。
 *
 * 【控制目標】
 * - 維持水箱液位在高低液位之間
 * - 低液位觸發補水 → 運行至高液位 → 停止補水
 * - 防止過度補水和缺水
 *
 * 【感測器配置】
 * - 高液位檢測 (REG 411112): DI_3, 1=有液位, 0=無液位
 * - //低液位檢測 (REG 411113): DI_4, 1=有液位, 0=無液位
 * - 漏液檢測 (REG 411114): DI_5, 1=漏液, 0=正常
 * - 系統狀態 (REG 42001): bit7=異常標誌
 *
 * 【執行器控制】
 * - 補水泵啟停 (REG 411108): DO_7, 1=運行, 0=停止
 *
 * 【控制模式】
 * - 手動模式 (WATER_PUMP_MODE_MANUAL): 僅監控,外部手動控制
 * - 自動模式 (WATER_PUMP_MODE_AUTO): 根據液位自動補水
 *
 * 【自動補水邏輯】
 * 1. 偵測低液位且非高液位 → 啟動延遲 → 開始補水
 * 2. 運行中監控: 高液位到達/漏液偵測/系統異常/運行超時
 * 3. 補水完成 → 停機延遲 → 進入待機
 * 4. 超時處理: 記錄失敗次數,達到上限停止自動補水
 *
 * 【運行狀態機】
 * - IDLE: 閒置,監控液位
 * - STARTING: 啟動延遲中
 * - RUNNING: 補水運行中
 * - STOPPING: 停止中
 * - COMPLETED: 補水完成
 * - TIMEOUT: 運行超時
 * - ERROR: 錯誤狀態
 *
 * 【配置參數】
 * - 目標壓力 (REG 45051): 補水壓力設定 (0.1 bar 精度)
 * - 啟動延遲 (REG 45052): 預設 2.0秒 (0.1s 單位)
 * - 最大運行時間 (REG 45053): 預設 300秒 (0.1s 單位)
 * - 完成延遲 (REG 45054): 預設 5.0秒 (0.1s 單位)
 * - 缺水警告延遲 (REG 45055): 預設 10.0秒 (0.1s 單位)
 * - 最大失敗次數 (REG 45056): 預設 3次
 * - 當前失敗次數 (REG 42801): 補水未滿累計次數
 *
 * 【安全保護】
 * - 液位確認延遲: 0.5秒 (防抖動)
 * - 漏液立即停機
 * - 系統異常立即停機
 * - 運行超時保護 (預設300秒)
 * - 失敗次數限制 (預設3次後停止自動補水)
 *
 * 【異常處理】
 * - 補水超時 → 停機 → 失敗次數+1 → 等待錯誤清除
 * - 漏液偵測 → 緊急停機 → 進入錯誤狀態
 * - 系統異常 → 停機 → 進入錯誤狀態
 * - 補水成功 → 失敗次數歸零
 *
 */

#include "dexatek/main_application/include/application_common.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"

// cdu_water_pump_control.c

// ========================================================================================
// CDU 補水泵寄存器定義 (依據 CDU 控制系統 Modbus 寄存器定義表)
// ========================================================================================

static const char* tag = "ls80_5_water_pump";

#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_ls80_5.json"
#define CONFIG_REGISTER_LIST_SIZE 15
static control_logic_register_t _control_logic_register_list[CONFIG_REGISTER_LIST_SIZE];

// 補水泵控制寄存器
static uint32_t REG_CONTROL_LOGIC_5_ENABLE = 41005; // 控制邏輯5啟用

static uint32_t REG_WATER_PUMP_CONTROL = 411003;   // 補水泵啟停控制 (0=Stop, 1=Run)

// 液位檢測寄存器
static uint32_t REG_HIGH_LEVEL = 411015;   // CDU水箱_高液位檢 (0=無液位, 1=有液位)
//static uint32_t REG_LOW_LEVEL = 411113;   // CDU水箱_低液位檢 (0=無液位, 1=有液位)
static uint32_t REG_LEAK_DETECTION = 411010;   // 漏液檢 (0=正常, 1=漏液)
static uint32_t REG_SYSTEM_STATUS = 42001;    // 機組狀態 (bit8:液位狀態)
static uint32_t REG_P5_PRESSURE = 42086; // P3壓力 11163, port 1, AI_B

// 設定參數寄存器
static uint32_t REG_TARGET_PRESSURE = 45051;    // 補水壓力設定 (bar)
static uint32_t REG_START_DELAY = 45052;    // 補水啟動延遲 (0.1s)
static uint32_t REG_MAX_RUN_TIME = 45053;    // 補水運行時間超時 (0.1s)
static uint32_t REG_COMPLETE_DELAY = 45054;    // 補水完成延遲 (0.1s)
static uint32_t REG_WARNING_DELAY = 45055;    // 缺水警告延遲 (0.1s)
static uint32_t REG_MAX_FAIL_COUNT = 45056;    // 補水未滿告警次數
static uint32_t REG_CURRENT_FAIL_COUNT = 42801;    // 補水未滿次數 (當前值)

// ========================================================================================
// 系統常數定義
// ========================================================================================

#define CONTROL_CYCLE_MS            1000     // 1秒控制週期
#define PUMP_RESPONSE_TIMEOUT_MS    2000     // 泵浦響應超時 (2秒)
#define LEVEL_CONFIRM_DELAY_MS      500      // 液位確認延遲 (0.5秒)

// 預設參數值
#define DEFAULT_START_DELAY         20       // 預設啟動延遲 2.0秒
#define DEFAULT_MAX_RUN_TIME        3000     // 預設最大運行時間 300秒
#define DEFAULT_COMPLETE_DELAY      50       // 預設完成延遲 5.0秒
#define DEFAULT_WARNING_DELAY       100      // 預設缺水警告延遲 10.0秒
#define DEFAULT_MAX_FAIL_COUNT      3        // 預設最大失敗次數

// ========================================================================================
// 資料結構定義
// ========================================================================================

// 控制模式
typedef enum {
    WATER_PUMP_MODE_AUTO = 0,     // 自動模式
    WATER_PUMP_MODE_MANUAL        // 手動模式
} water_pump_mode_t;

// 運行狀態
typedef enum {
    WATER_PUMP_STATE_IDLE = 0,    // 閒置
    WATER_PUMP_STATE_STARTING,    // 啟動中
    WATER_PUMP_STATE_RUNNING,     // 運行中
    WATER_PUMP_STATE_STOPPING,   // 停止中
    WATER_PUMP_STATE_COMPLETED,   // 完成
    WATER_PUMP_STATE_TIMEOUT,     // 超時
    WATER_PUMP_STATE_ERROR        // 錯誤
} water_pump_state_t;

// 補水泵配置
typedef struct {
    float target_pressure;        // 目標壓力 (bar)
    uint32_t start_delay_ms;      // 啟動延遲 (毫秒)
    uint32_t max_run_time_ms;     // 最大運行時間 (毫秒)
    uint32_t complete_delay_ms;   // 完成延遲 (毫秒)
    uint32_t warning_delay_ms;    // 缺水警告延遲 (毫秒)
    uint32_t max_fail_count;      // 最大失敗次數
} water_pump_config_t;

// 補水泵狀態
typedef struct {
    bool is_running;              // 補水泵是否運行
    bool high_level;              // 高液位狀態
    bool low_level;               // 低液位狀態
    bool leak_detected;           // 漏液檢測
    bool system_normal;           // 系統正常
    uint32_t current_fail_count;  // 當前失敗次數
    
    // 運行時狀態
    uint32_t start_time_ms;       // 開始時間
    uint32_t last_level_check_ms; // 上次液位檢查時間
    bool level_confirmed;         // 液位確認狀態
} water_pump_status_t;

// 主控制器結構
typedef struct {
    // 控制模式和狀態
    water_pump_mode_t control_mode;
    water_pump_state_t pump_state;
    
    // 配置和狀態
    water_pump_config_t config;
    water_pump_status_t status;
    
    // 系統狀態
    bool system_initialized;
    uint32_t cycle_count;
    uint32_t comm_error_count;
    
} water_pump_controller_t;

// ========================================================================================
// 函數原型宣告
// ========================================================================================

// 系統資料讀寫
static bool read_water_pump_config(water_pump_config_t* config);
static bool read_water_pump_status(water_pump_status_t* status);
static bool write_pump_control(bool enable);
static bool write_fail_count(uint32_t count);

// 安全檢查和控制邏輯
static bool check_safety_conditions(water_pump_status_t* status);
static bool confirm_level_status(water_pump_status_t* status);
static void start_water_pump(water_pump_controller_t* controller);
static void stop_water_pump(water_pump_controller_t* controller);
static void handle_pump_timeout(water_pump_controller_t* controller);

// 控制模式函數
static void execute_manual_control(water_pump_controller_t* controller);
static void execute_auto_control(water_pump_controller_t* controller, uint32_t current_time_ms);

// Modbus通信函數
static uint16_t read_holding_register(uint32_t address);
static bool write_holding_register(uint32_t address, uint16_t value);

// ========================================================================================
// 全域變數
// ========================================================================================

static water_pump_controller_t _water_pump_controller = {0};

// ========================================================================================
// Modbus通信實現
// ========================================================================================

static uint16_t read_holding_register(uint32_t address) {
    // TODO: 透過 control_hardware 模組實現實際的 Modbus 讀取

    // 暫時返回模擬值用於測試
    // switch (address) {
    //     case REG_WATER_PUMP_CONTROL: return 0;     // 補水泵停止
    //     case REG_HIGH_LEVEL:         return 0;     // 無高液位
    //     case REG_LOW_LEVEL:          return 1;     // 有低液位
    //     case REG_LEAK_DETECTION:     return 0;     // 無漏液
    //     case REG_SYSTEM_STATUS:      return 0x102; // 系統正常運行
    //     case REG_TARGET_PRESSURE:    return 25;    // 2.5 bar
    //     case REG_START_DELAY:        return DEFAULT_START_DELAY;
    //     case REG_MAX_RUN_TIME:       return DEFAULT_MAX_RUN_TIME;
    //     case REG_COMPLETE_DELAY:     return DEFAULT_COMPLETE_DELAY;
    //     case REG_WARNING_DELAY:      return DEFAULT_WARNING_DELAY;
    //     case REG_MAX_FAIL_COUNT:     return DEFAULT_MAX_FAIL_COUNT;
    //     case REG_CURRENT_FAIL_COUNT: return 0;     // 無失敗次數
    //     default: return 0;
    // }

    uint16_t value = 0;
    int ret = control_logic_read_holding_register(address, &value);

    if (ret != SUCCESS) {
        value = 0xFFFF;
    }

    return value;    

}

static bool write_holding_register(uint32_t address, uint16_t value) {
    // TODO: 透過 control_hardware 模組實現實際的 Modbus 寫入
    // debug(tag, "Write register @0x%04X = %d", address, value);

    int ret = control_logic_write_register(address, value, 2000);

    return (ret == SUCCESS)? true : false;
}

// ========================================================================================
// 系統資料讀寫
// ========================================================================================

// 讀取補水泵配置
static bool read_water_pump_config(water_pump_config_t* config) {
    uint16_t pressure_raw = read_holding_register(REG_TARGET_PRESSURE);
    uint16_t start_delay_raw = read_holding_register(REG_START_DELAY);
    uint16_t max_run_time_raw = read_holding_register(REG_MAX_RUN_TIME);
    uint16_t complete_delay_raw = read_holding_register(REG_COMPLETE_DELAY);
    uint16_t warning_delay_raw = read_holding_register(REG_WARNING_DELAY);
    uint16_t max_fail_count_raw = read_holding_register(REG_MAX_FAIL_COUNT);
    
    debug(tag, "pressure_raw = %d (HMI)(%d)", pressure_raw, REG_TARGET_PRESSURE);
    debug(tag, "start_delay_raw = %d (HMI)(%d)", start_delay_raw, REG_START_DELAY);
    debug(tag, "max_run_time_raw = %d (HMI)(%d)", max_run_time_raw, REG_MAX_RUN_TIME);
    debug(tag, "complete_delay_raw = %d (HMI)(%d)", complete_delay_raw, REG_COMPLETE_DELAY);
    debug(tag, "warning_delay_raw = %d (HMI)(%d)", warning_delay_raw, REG_WARNING_DELAY);
    debug(tag, "max_fail_count_raw = %d (HMI)(%d)", max_fail_count_raw, REG_MAX_FAIL_COUNT);

    if (pressure_raw != 0xFFFF) {
        config->target_pressure = (float)pressure_raw / 10.0f; // 0.1bar精度
    }
    
    config->start_delay_ms = (start_delay_raw != 0xFFFF) ? 
                            (start_delay_raw * 100) : (DEFAULT_START_DELAY * 100);
    config->max_run_time_ms = (max_run_time_raw != 0xFFFF) ? 
                             (max_run_time_raw * 100) : (DEFAULT_MAX_RUN_TIME * 100);
    config->complete_delay_ms = (complete_delay_raw != 0xFFFF) ? 
                               (complete_delay_raw * 100) : (DEFAULT_COMPLETE_DELAY * 100);
    config->warning_delay_ms = (warning_delay_raw != 0xFFFF) ? 
                              (warning_delay_raw * 100) : (DEFAULT_WARNING_DELAY * 100);
    config->max_fail_count = (max_fail_count_raw != 0xFFFF) ? 
                            max_fail_count_raw : DEFAULT_MAX_FAIL_COUNT;
    
    return true;
}

// 讀取補水泵狀態
static bool read_water_pump_status(water_pump_status_t* status) {
    uint16_t pump_control = read_holding_register(REG_WATER_PUMP_CONTROL);
    uint16_t high_level = read_holding_register(REG_HIGH_LEVEL);
    //uint16_t low_level = read_holding_register(REG_LOW_LEVEL);
    uint16_t leak_detection = read_holding_register(REG_LEAK_DETECTION);
    uint16_t system_status = read_holding_register(REG_SYSTEM_STATUS);
    uint16_t fail_count = read_holding_register(REG_CURRENT_FAIL_COUNT);
    
    //debug(tag, "low_level = %d (DI_4)(%d)", low_level, REG_LOW_LEVEL);
    debug(tag, "leak_detection = %d (DI_5)(%d)", leak_detection, REG_LEAK_DETECTION);
    debug(tag, "system_status = %d (HMI)(%d)", system_status, REG_SYSTEM_STATUS);
    debug(tag, "fail_count = %d (HMI)(%d)", fail_count, REG_CURRENT_FAIL_COUNT);

    bool success = (pump_control != 0xFFFF) && (high_level != 0xFFFF) &&
                   (leak_detection != 0xFFFF) &&
                   (system_status != 0xFFFF);

    if (success) {
        status->is_running = (pump_control != 0);
        status->high_level = (high_level != 0);
        status->low_level = false; // REG_LOW_LEVEL not used in this version
        status->leak_detected = (leak_detection != 0);
        status->system_normal = ((system_status & 0x80) == 0); // bit7: 異常
        status->current_fail_count = (fail_count != 0xFFFF) ? fail_count : 0;
    }
    
    return success;
}

// 寫入泵浦控制指令
static bool write_pump_control(bool enable) {
    uint16_t cmd_value = enable ? 1 : 0;
    bool result = write_holding_register(REG_WATER_PUMP_CONTROL, cmd_value);
    
    if (result) {
        info(tag, "Water pump %s", enable ? "STARTED" : "STOPPED");
    } else {
        error(tag, "Failed to %s water pump", enable ? "start" : "stop");
    }
    
    return result;
}

// 寫入失敗次數
static bool write_fail_count(uint32_t count) {
    return write_holding_register(REG_CURRENT_FAIL_COUNT, (uint16_t)count);
}

// ========================================================================================
// 安全檢查和控制邏輯
// ========================================================================================

// 安全條件檢查
static bool check_safety_conditions(water_pump_status_t* status) {
    // 檢查系統狀態
    if (!status->system_normal) {
        debug(tag, "Safety check failed: System abnormal");
        return false;
    }
    
    // 檢查漏液
    if (status->leak_detected) {
        debug(tag, "Safety check failed: Leak detected");
        return false;
    }
    
    // 檢查高液位（已經滿水就不需要補水）
    if (status->high_level) {
        debug(tag, "Safety check failed: High level already reached");
        return false;
    }
    
    return true;
}

// 確認液位狀態（防止液位開關抖動）
static bool confirm_level_status(water_pump_status_t* status) {
    uint32_t current_time_ms = time32_get_current_ms();
    
    if (!status->level_confirmed) {
        status->last_level_check_ms = current_time_ms;
        status->level_confirmed = true;
        return false; // 第一次檢查，需要等待確認
    }
    
    if ((current_time_ms - status->last_level_check_ms) >= LEVEL_CONFIRM_DELAY_MS) {
        return true; // 延遲時間已達，狀態確認
    }
    
    return false; // 還在等待確認
}

// 啟動補水泵
static void start_water_pump(water_pump_controller_t* controller) {
    water_pump_config_t* config = &controller->config;
    uint32_t current_time_ms = time32_get_current_ms();
    
    info(tag, "Starting water pump (delay: %dms)", config->start_delay_ms);
    
    // 設定啟動狀態
    controller->pump_state = WATER_PUMP_STATE_STARTING;
    controller->status.start_time_ms = current_time_ms + config->start_delay_ms;
    
    // 如果沒有啟動延遲，直接啟動
    if (config->start_delay_ms == 0) {
        write_pump_control(true);
        controller->pump_state = WATER_PUMP_STATE_RUNNING;
        controller->status.start_time_ms = current_time_ms;
    }
}

// 停止補水泵
static void stop_water_pump(water_pump_controller_t* controller) {
    info(tag, "Stopping water pump");
    
    write_pump_control(false);
    controller->pump_state = WATER_PUMP_STATE_STOPPING;
    controller->status.start_time_ms = 0;
}

// 處理泵浦超時
static void handle_pump_timeout(water_pump_controller_t* controller) {
    warn(tag, "Water pump timeout - stopping pump");
    
    stop_water_pump(controller);
    controller->pump_state = WATER_PUMP_STATE_TIMEOUT;
    
    // 增加失敗次數
    controller->status.current_fail_count++;
    write_fail_count(controller->status.current_fail_count);
    
    warn(tag, "Water filling failed (%d/%d)", 
            controller->status.current_fail_count, controller->config.max_fail_count);
}

// ========================================================================================
// 控制邏輯實現
// ========================================================================================

// 手動控制模式
static void execute_manual_control(water_pump_controller_t* controller) {
    // 手動模式下主要進行監控，實際控制由外部 HMI 或 SCADA 系統進行
    water_pump_status_t* status = &controller->status;
    
    // 監控補水泵狀態
    if (status->is_running) {
        // 檢查是否需要停止（安全檢查）
        if (status->leak_detected) {
            warn(tag, "Manual mode: Leak detected - recommend stopping pump");
        }
        
        if (status->high_level) {
            info(tag, "Manual mode: High level reached - recommend stopping pump");
        }
        
        debug(tag, "Manual mode: Pump running - monitoring");
    } else {
        debug(tag, "Manual mode: Pump stopped - monitoring");
    }
}

// 自動控制模式
static void execute_auto_control(water_pump_controller_t* controller, uint32_t current_time_ms) {
    water_pump_config_t* config = &controller->config;
    water_pump_status_t* status = &controller->status;
    
    switch (controller->pump_state) {
        case WATER_PUMP_STATE_IDLE:
            // 檢查是否需要開始補水
            if (status->low_level && !status->high_level) {
                if (confirm_level_status(status)) {
                    // 液位狀態確認，進行安全檢查
                    if (check_safety_conditions(status)) {
                        if (status->current_fail_count < config->max_fail_count) {
                            start_water_pump(controller);
                        } else {
                            warn(tag, "Auto mode: Max fail count reached (%d), skipping start", 
                                   config->max_fail_count);
                        }
                    } else {
                        debug(tag, "Auto mode: Safety check failed, cannot start pump");
                    }
                    status->level_confirmed = false; // 重置確認狀態
                }
            } else {
                status->level_confirmed = false; // 重置確認狀態
            }
            break;
            
        case WATER_PUMP_STATE_STARTING:
            // 檢查是否到了啟動時間
            if (current_time_ms >= status->start_time_ms) {
                write_pump_control(true);
                controller->pump_state = WATER_PUMP_STATE_RUNNING;
                status->start_time_ms = current_time_ms;
                info(tag, "Auto mode: Pump started");
            }
            break;
            
        case WATER_PUMP_STATE_RUNNING:
            // 監控補水過程
            if (!status->is_running) {
                warn(tag, "Auto mode: Pump unexpectedly stopped");
                controller->pump_state = WATER_PUMP_STATE_ERROR;
                break;
            }
            
            // 檢查是否達到高液位
            if (status->high_level) {
                if (confirm_level_status(status)) {
                    info(tag, "Auto mode: High level reached, stopping pump");
                    stop_water_pump(controller);
                    controller->pump_state = WATER_PUMP_STATE_COMPLETED;
                    status->level_confirmed = false;
                }
                break;
            }
            
            // 檢查安全條件
            if (status->leak_detected) {
                warn(tag, "Auto mode: Leak detected, emergency stop");
                stop_water_pump(controller);
                controller->pump_state = WATER_PUMP_STATE_ERROR;
                break;
            }
            
            if (!status->system_normal) {
                warn(tag, "Auto mode: System abnormal, stopping pump");
                stop_water_pump(controller);
                controller->pump_state = WATER_PUMP_STATE_ERROR;
                break;
            }
            
            // 檢查超時
            if ((current_time_ms - status->start_time_ms) >= config->max_run_time_ms) {
                handle_pump_timeout(controller);
            }
            
            status->level_confirmed = false; // 運行中重置確認狀態
            break;
            
        case WATER_PUMP_STATE_STOPPING:
            // 確認泵浦已停止
            if (!status->is_running) {
                info(tag, "Auto mode: Pump stopped successfully");
                controller->pump_state = WATER_PUMP_STATE_IDLE;
            }
            break;
            
        case WATER_PUMP_STATE_COMPLETED:
            // 補水完成，等待延遲時間
            if ((current_time_ms - status->start_time_ms) >= config->complete_delay_ms) {
                info(tag, "Auto mode: Water filling completed successfully");
                // 重置失敗次數
                if (status->current_fail_count > 0) {
                    controller->status.current_fail_count = 0;
                    write_fail_count(0);
                    info(tag, "Auto mode: Fail count reset");
                }
                controller->pump_state = WATER_PUMP_STATE_IDLE;
            }
            break;
            
        case WATER_PUMP_STATE_TIMEOUT:
        case WATER_PUMP_STATE_ERROR:
            // 錯誤狀態，等待一段時間後重新開始
            if ((current_time_ms - status->start_time_ms) >= (config->warning_delay_ms * 2)) {
                controller->pump_state = WATER_PUMP_STATE_IDLE;
                info(tag, "Auto mode: Returning to idle state after error");
            }
            break;
    }
}

// ========================================================================================
// 主要函數
// ========================================================================================

static int _register_list_init(void)
{
    int ret = SUCCESS;

    _control_logic_register_list[0].name = REG_CONTROL_LOGIC_5_ENABLE_STR;
    _control_logic_register_list[0].address_ptr = &REG_CONTROL_LOGIC_5_ENABLE;
    _control_logic_register_list[0].default_address = REG_CONTROL_LOGIC_5_ENABLE;
    _control_logic_register_list[0].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[1].name = REG_WATER_PUMP_CONTROL_STR;
    _control_logic_register_list[1].address_ptr = &REG_WATER_PUMP_CONTROL;
    _control_logic_register_list[1].default_address = REG_WATER_PUMP_CONTROL;
    _control_logic_register_list[1].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[2].name = REG_HIGH_LEVEL_STR;
    _control_logic_register_list[2].address_ptr = &REG_HIGH_LEVEL;
    _control_logic_register_list[2].default_address = REG_HIGH_LEVEL;
    _control_logic_register_list[2].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[3].name = REG_P5_PRESSURE_STR;
    _control_logic_register_list[3].address_ptr = &REG_P5_PRESSURE, 
    _control_logic_register_list[3].default_address = REG_P5_PRESSURE,
    _control_logic_register_list[3].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[4].name = REG_LEAK_DETECTION_STR;
    _control_logic_register_list[4].address_ptr = &REG_LEAK_DETECTION;
    _control_logic_register_list[4].default_address = REG_LEAK_DETECTION;
    _control_logic_register_list[4].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[5].name = REG_SYSTEM_STATUS_STR;
    _control_logic_register_list[5].address_ptr = &REG_SYSTEM_STATUS;
    _control_logic_register_list[5].default_address = REG_SYSTEM_STATUS;
    _control_logic_register_list[5].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[6].name = REG_TARGET_PRESSURE_STR;
    _control_logic_register_list[6].address_ptr = &REG_TARGET_PRESSURE;
    _control_logic_register_list[6].default_address = REG_TARGET_PRESSURE;
    _control_logic_register_list[6].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[7].name = REG_START_DELAY_STR;
    _control_logic_register_list[7].address_ptr = &REG_START_DELAY;
    _control_logic_register_list[7].default_address = REG_START_DELAY;
    _control_logic_register_list[7].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[8].name = REG_MAX_RUN_TIME_STR;
    _control_logic_register_list[8].address_ptr = &REG_MAX_RUN_TIME;
    _control_logic_register_list[8].default_address = REG_MAX_RUN_TIME;
    _control_logic_register_list[8].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[9].name = REG_COMPLETE_DELAY_STR;
    _control_logic_register_list[9].address_ptr = &REG_COMPLETE_DELAY;
    _control_logic_register_list[9].default_address = REG_COMPLETE_DELAY;
    _control_logic_register_list[9].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[10].name = REG_WARNING_DELAY_STR;
    _control_logic_register_list[10].address_ptr = &REG_WARNING_DELAY;
    _control_logic_register_list[10].default_address = REG_WARNING_DELAY;
    _control_logic_register_list[10].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[11].name = REG_MAX_FAIL_COUNT_STR;
    _control_logic_register_list[11].address_ptr = &REG_MAX_FAIL_COUNT;
    _control_logic_register_list[11].default_address = REG_MAX_FAIL_COUNT;
    _control_logic_register_list[11].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[12].name = REG_CURRENT_FAIL_COUNT_STR;
    _control_logic_register_list[12].address_ptr = &REG_CURRENT_FAIL_COUNT;
    _control_logic_register_list[12].default_address = REG_CURRENT_FAIL_COUNT;
    _control_logic_register_list[12].type = CONTROL_LOGIC_REGISTER_READ;

    // try to load register array from file
    uint32_t list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    ret = control_logic_register_load_from_file(CONFIG_REGISTER_FILE_PATH, _control_logic_register_list, list_size);
    debug(tag, "load register array from file %s, ret %d", CONFIG_REGISTER_FILE_PATH, ret);

    return ret;
}

int control_logic_ls80_5_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

// 初始化函數
int control_logic_ls80_5_waterpump_control_init(void) {
    info(tag, "Initializing CDU water pump controller");
    
    // register list init
    _register_list_init();
    
    // 初始化控制器
    memset(&_water_pump_controller, 0, sizeof(_water_pump_controller));
    
    // 設定預設狀態
    _water_pump_controller.control_mode = WATER_PUMP_MODE_AUTO;
    _water_pump_controller.pump_state = WATER_PUMP_STATE_IDLE;
    
    // 載入預設配置
    _water_pump_controller.config.target_pressure = 2.5f;
    _water_pump_controller.config.start_delay_ms = DEFAULT_START_DELAY * 100;
    _water_pump_controller.config.max_run_time_ms = DEFAULT_MAX_RUN_TIME * 100;
    _water_pump_controller.config.complete_delay_ms = DEFAULT_COMPLETE_DELAY * 100;
    _water_pump_controller.config.warning_delay_ms = DEFAULT_WARNING_DELAY * 100;
    _water_pump_controller.config.max_fail_count = DEFAULT_MAX_FAIL_COUNT;

    // 從 Modbus 表載入持久化的配置數值（如果存在會覆蓋預設值）
    info(tag, "Loading persisted configuration from Modbus registers");
    read_water_pump_config(&_water_pump_controller.config);

    // 記錄載入的配置
    info(tag, "Config loaded: pressure=%.1f bar, start_delay=%d ms, max_run=%d ms, complete_delay=%d ms",
         _water_pump_controller.config.target_pressure,
         _water_pump_controller.config.start_delay_ms,
         _water_pump_controller.config.max_run_time_ms,
         _water_pump_controller.config.complete_delay_ms);

    _water_pump_controller.system_initialized = true;

    info(tag, "CDU water pump controller initialized successfully");
    return 0;
}

// 主控制函數 - 整合到 control_logic_X 框架
int control_logic_ls80_5_waterpump_control(ControlLogic *ptr) {
    if (ptr == NULL) return -1;
    
    // check enable
    if (read_holding_register(REG_CONTROL_LOGIC_5_ENABLE) != 1) {
        return 0;
    }
    
    uint32_t current_time_ms = time32_get_current_ms();
    
    debug(tag, "Water pump control cycle %u", current_time_ms);
    
    // 讀取配置
    if (!read_water_pump_config(&_water_pump_controller.config)) {
        error(tag, "Failed to read water pump configuration");
        _water_pump_controller.comm_error_count++;
        return -1;
    }
    
    // 讀取狀態
    if (!read_water_pump_status(&_water_pump_controller.status)) {
        error(tag, "Failed to read water pump status");
        _water_pump_controller.comm_error_count++;
        return -1;
    }
    
    // 根據控制模式執行對應邏輯
    if (_water_pump_controller.control_mode == WATER_PUMP_MODE_MANUAL) {
        // 手動模式
        execute_manual_control(&_water_pump_controller);
    } else {
        // 自動模式
        execute_auto_control(&_water_pump_controller, current_time_ms);
    }
    
    // 更新統計
    _water_pump_controller.cycle_count++;
    
    return 0;
}

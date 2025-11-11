/*
 * control_logic_lx1400_6.c - LX1400 比例閥控制邏輯 (Control Logic 6: Valve Control)
 *
 * 【功能概述】
 * 本模組實現 LX1400 機型的比例閥控制功能,通過 PID 演算法控制閥門開度以維持流量穩定。
 * 使用 4-20mA 類比輸出控制閥門開度,並讀取 4-20mA 類比輸入獲取實際開度回饋。
 *
 * 【控制目標】
 * - 維持 F2 流量在設定值 (預設 100 L/min)
 * - 通過調整閥門開度實現流量控制
 *
 * 【感測器配置】
 * - F2 流量 (REG 42063): 二次側出水流量
 * - P11 壓力 (REG 42092): 進水壓力 (參考)
 * - P17 壓力 (REG 42098): 出水壓力 (參考)
 * - 閥門開度回饋 (REG 411161): 實際開度 0-100% (4-20mA 輸入)
 *
 * 【執行器控制】
 * - 閥門命令開度 (REG 411147): 設定開度 0-100% (4-20mA 輸出)
 * - 手動/自動模式 (REG 45061): 0=自動, 1=手動
 *
 * 【控制模式】
 * - 手動模式: 操作員直接設定閥門開度
 * - 自動模式: PID 控制閥門開度以維持流量
 *
 * 【PID 參數】
 * - Kp: 1.0 (比例增益)
 * - Ki: 0.1 (積分增益)
 * - Kd: 0.05 (微分增益)
 *
 * 【閥門控制策略】
 * - 單次最大調整量: 10%
 * - 響應容差: ±2%
 *
 * 【安全保護】
 * - 閥門開度限制: 0-100%
 * - 緊急流量保護: F2 < 50 L/min 或 > 200 L/min 時告警
 * - 閥門故障檢測: 命令vs實際開度差異 > 5% 超過10秒則告警
 *
 * 【閥門開度轉換】
 * - 寫入: 開度% → 4-20mA (線性轉換)
 * - 讀取: 4-20mA → 開度% (線性轉換)
 *
 * 作者: LX1400 機型適配
 * 日期: 2025
 */

#include "dexatek/main_application/include/application_common.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"
// proportional_valve_control.c

// 比例閥控制邏輯程式
// ========================================================================================
// 比例閥控制寄存器定義 (依據 CDU 控制系統 Modbus 寄存器定義表)
// ========================================================================================

static const char* tag = "lx1400_6_valve";  // 日誌標籤

#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_lx1400_6.json"
#define CONFIG_REGISTER_LIST_SIZE 15
static control_logic_register_t _control_logic_register_list[CONFIG_REGISTER_LIST_SIZE];


// 比例閥控制寄存器
static uint32_t REG_CONTROL_LOGIC_6_ENABLE = 41006; // 控制邏輯6啟用

static uint32_t REG_VALVE_STATE = 411161;   // 電磁閥開度回饋值 (%)
static uint32_t REG_VALVE_COMMAND = 411147;   // 電磁閥命令開度設定值 (%)
static uint32_t REG_VALVE_MANUAL = 45061;    // 電磁閥手動模式 (0=自動, 1=手動)

// 控制設定值寄存器
static uint32_t REG_FLOW_SETPOINT = 45003;    // 目標控制流量 (L/min)
static uint32_t REG_CONTROL_MODE = 45005;    // 流量模式(0)/壓差模式(1) - 僅使用流量模式
static uint32_t REG_FLOW_HIGH_LIMIT = 45006;    // 流量上限 (L/min)
static uint32_t REG_FLOW_LOW_LIMIT = 45007;    // 流量下限 (L/min)

// 感測器反饋寄存器
static uint32_t REG_F2_FLOW = 42063;    // F2二次側出水流量 (0.1L/min)
static uint32_t REG_P11_PRESSURE = 42092;    // P11二次側進水壓力 (bar)
static uint32_t REG_P17_PRESSURE = 42098;    // P17二次側出水壓力 (bar)

// ========================================================================================
// 系統常數定義
// ========================================================================================

#define VALVE_MIN_OPENING           0.0f     // 最小閥門開度 (%)
#define VALVE_MAX_OPENING           100.0f   // 最大閥門開度 (%)
#define VALVE_MAX_ADJUSTMENT        10.0f    // 單次最大調整量 (%)
#define VALVE_RESPONSE_TOLERANCE    2.0f     // 閥門響應容差 (%)
#define EMERGENCY_REDUCTION         10.0f    // 緊急流量減小量 (%)
#define EMERGENCY_INCREASE          5.0f     // 緊急流量增加量 (%)
#define CONTROL_CYCLE_MS            1000     // 1秒控制週期

// PID控制參數 - 流量控制模式
#define FLOW_PID_KP                 1.0f     // 比例增益
#define FLOW_PID_KI                 0.1f     // 積分增益
#define FLOW_PID_KD                 0.05f    // 微分增益
#define PID_OUTPUT_MIN              -10.0f   // PID輸出下限
#define PID_OUTPUT_MAX              10.0f    // PID輸出上限
#define PID_INTEGRAL_MAX            100.0f   // 積分限制

// ========================================================================================
// 資料結構定義
// ========================================================================================

// PID控制器
typedef struct {
    float kp, ki, kd;              // PID參數
    float setpoint;                // 設定值
    float integral;                // 積分項
    float last_error;              // 上次誤差
    float output;                  // 輸出值
    float output_min, output_max;  // 輸出限制
    uint32_t last_time_ms;         // 上次計算時間
    bool enabled;                  // 是否啟用
} pid_controller_t;

// 閥門控制配置
typedef struct {
    bool manual_mode;              // 手動模式 (REG_VALVE_MANUAL)
    float target_flow;             // 目標流量 (REG_FLOW_SETPOINT)
    float flow_high_limit;         // 流量上限 (REG_FLOW_HIGH_LIMIT)
    float flow_low_limit;          // 流量下限 (REG_FLOW_LOW_LIMIT)
    float manual_setpoint;         // 手動模式設定開度
} valve_config_t;

// 閥門狀態資料
typedef struct {
    float current_opening;         // 當前實際開度 (REG_VALVE_STATE)
    float command_opening;         // 命令開度 (REG_VALVE_COMMAND)
    float actual_flow;             // 實際流量 (REG_F2_FLOW)
    float inlet_pressure;          // 進水壓力 (REG_P11_PRESSURE)
    float outlet_pressure;         // 出水壓力 (REG_P17_PRESSURE)
    bool valve_fault;              // 閥門故障標誌
    uint32_t fault_count;          // 連續故障次數
} valve_status_t;

// 主控制器結構
typedef struct {
    // PID控制器
    pid_controller_t flow_pid;
    
    // 配置和狀態
    valve_config_t config;
    valve_status_t status;
    
    // 系統狀態
    bool system_initialized;
    uint32_t cycle_count;
    uint32_t comm_error_count;
    
} valve_controller_t;

// ========================================================================================
// 函數原型宣告
// ========================================================================================

// PID控制函數
static void pid_init(pid_controller_t* pid, float kp, float ki, float kd);
static float pid_calculate(pid_controller_t* pid, float process_value, uint32_t current_time_ms);
// static void pid_reset(pid_controller_t* pid);

// 系統資料讀寫
static bool read_valve_config(valve_config_t* config);
static bool read_valve_status(valve_status_t* status);
static bool write_valve_command(float opening_percent);

// 安全限制和保護
static float apply_safety_limits(float value);
static bool check_valve_response(float command_opening, float actual_opening);
static void handle_valve_fault(valve_controller_t* controller);
static float emergency_flow_protection(float current_opening, float actual_flow, 
                                     float high_limit, float low_limit);

// 控制邏輯函數
static void execute_manual_control(valve_controller_t* controller);
static void execute_flow_control(valve_controller_t* controller, uint32_t current_time_ms);

// Modbus通信函數
static uint16_t read_holding_register(uint32_t address);
static bool write_holding_register(uint32_t address, uint16_t value);

// ========================================================================================
// 全域變數
// ========================================================================================

static valve_controller_t _valve_controller;

// ========================================================================================
// PID控制實現
// ========================================================================================

// 初始化PID控制器
static void pid_init(pid_controller_t* pid, float kp, float ki, float kd) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->setpoint = 0.0f;
    pid->integral = 0.0f;
    pid->last_error = 0.0f;
    pid->output = 0.0f;
    pid->output_min = PID_OUTPUT_MIN;
    pid->output_max = PID_OUTPUT_MAX;
    pid->last_time_ms = 0;
    pid->enabled = false;
}

// PID計算
static float pid_calculate(pid_controller_t* pid, float process_value, uint32_t current_time_ms) {
    if (!pid->enabled) return 0.0f;
    
    if (pid->last_time_ms == 0) {
        pid->last_time_ms = current_time_ms;
        return 0.0f;
    }
    
    float dt = (current_time_ms - pid->last_time_ms) / 1000.0f; // 轉換為秒
    if (dt <= 0.0f) return pid->output;
    
    float error = pid->setpoint - process_value;
    
    // 比例項
    float proportional = pid->kp * error;
    
    // 積分項
    pid->integral += error * dt;
    
    // 積分限幅防止飽和
    if (pid->integral > PID_INTEGRAL_MAX) pid->integral = PID_INTEGRAL_MAX;
    if (pid->integral < -PID_INTEGRAL_MAX) pid->integral = -PID_INTEGRAL_MAX;
    
    float integral = pid->ki * pid->integral;
    
    // 微分項
    float derivative = pid->kd * (error - pid->last_error) / dt;
    pid->last_error = error;
    
    // PID輸出
    pid->output = proportional + integral + derivative;
    
    // 輸出限幅
    if (pid->output > pid->output_max) pid->output = pid->output_max;
    if (pid->output < pid->output_min) pid->output = pid->output_min;
    
    pid->last_time_ms = current_time_ms;
    
    return pid->output;
}

// 重置PID控制器
// static void pid_reset(pid_controller_t* pid) {
//     pid->integral = 0.0f;
//     pid->last_error = 0.0f;
//     pid->output = 0.0f;
//     pid->last_time_ms = 0;
// }

// ========================================================================================
// Modbus通信實現
// ========================================================================================

static uint16_t read_holding_register(uint32_t address) {
    // TODO: 透過 control_hardware 模組實現實際的 Modbus 讀取
    // 暫時返回模擬值用於測試
    // switch (address) {
    //     case REG_VALVE_STATE:     return 45;    // 45% 閥門開度
    //     case REG_VALVE_COMMAND:   return 45;    // 45% 命令開度
    //     case REG_VALVE_MANUAL:    return 0;     // 自動模式
    //     case REG_F2_FLOW:         return 1200;  // 120.0 L/min (0.1精度)
    //     case REG_FLOW_SETPOINT:   return 1000;  // 100.0 L/min設定值
    //     case REG_FLOW_HIGH_LIMIT: return 2000;  // 200.0 L/min上限
    //     case REG_FLOW_LOW_LIMIT:  return 500;   // 50.0 L/min下限
    //     case REG_P11_PRESSURE:    return 32;    // 3.2 bar
    //     case REG_P17_PRESSURE:    return 28;    // 2.8 bar
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

// 讀取閥門控制配置
static bool read_valve_config(valve_config_t* config) {
    uint16_t manual_mode_raw = read_holding_register(REG_VALVE_MANUAL);
    if (manual_mode_raw == 0xFFFF) return false;
    
    config->manual_mode = (manual_mode_raw != 0);
    
    if (!config->manual_mode) {
        // 自動模式下讀取控制參數
        uint16_t flow_sp_raw = read_holding_register(REG_FLOW_SETPOINT);
        uint16_t flow_high_raw = read_holding_register(REG_FLOW_HIGH_LIMIT);
        uint16_t flow_low_raw = read_holding_register(REG_FLOW_LOW_LIMIT);
        
        if (flow_sp_raw != 0xFFFF) {
            config->target_flow = (float)flow_sp_raw / 10.0f; // 0.1L/min換算
        }
        if (flow_high_raw != 0xFFFF) {
            config->flow_high_limit = (float)flow_high_raw / 10.0f;
        }
        if (flow_low_raw != 0xFFFF) {
            config->flow_low_limit = (float)flow_low_raw / 10.0f;
        }
    }
    
    return true;
}

// 讀取閥門狀態
static bool read_valve_status(valve_status_t* status) {
    bool success = true;
    
    // 讀取閥門開度
    uint16_t valve_state_raw = read_holding_register(REG_VALVE_STATE);
    if (valve_state_raw != 0xFFFF) {
        
        // Convert register value (4-20) to percentage (0-100)
        // Linear mapping: 4 -> 0, 20 -> 100
        status->current_opening = (float)(valve_state_raw - 4) * 100.0f / 16.0f;
        // status->current_opening = (float)valve_state_raw;

    } else {
        success = false;
    }
    
    uint16_t valve_cmd_raw = read_holding_register(REG_VALVE_COMMAND);
    if (valve_cmd_raw != 0xFFFF) {
        status->command_opening = (float)valve_cmd_raw;
    } else {
        success = false;
    }
    
    // 讀取流量
    uint16_t flow_raw = read_holding_register(REG_F2_FLOW);
    if (flow_raw != 0xFFFF) {
        status->actual_flow = (float)flow_raw / 10.0f; // 0.1L/min換算
    } else {
        success = false;
    }
    
    // 讀取壓力（用於監控，不參與控制）
    uint16_t p11_raw = read_holding_register(REG_P11_PRESSURE);
    uint16_t p17_raw = read_holding_register(REG_P17_PRESSURE);
    if (p11_raw != 0xFFFF) status->inlet_pressure = (float)p11_raw;
    if (p17_raw != 0xFFFF) status->outlet_pressure = (float)p17_raw;
    
    return success;
}

// 寫入閥門命令開度
static bool write_valve_command(float opening_percent) {
    // 確保開度在有效範圍內
    float safe_opening = apply_safety_limits(opening_percent);
    
    // uint16_t cmd_value = (uint16_t)safe_opening;
    
    // Convert percentage (0-100) to register value (4-20)
    // Linear mapping: 0% -> 4, 100% -> 20
    uint16_t cmd_value = (uint16_t)safe_opening;
    
    bool write_ret = write_holding_register(REG_VALVE_COMMAND, cmd_value);
    if (write_ret == true) {
        info(tag, "Valve command set to %.1f%%", safe_opening);
    } else {
        error(tag, "Failed to write valve command");
    }
        
    return write_ret;
}

// ========================================================================================
// 安全限制和保護
// ========================================================================================

// 應用安全限制
static float apply_safety_limits(float value) {
    if (value > VALVE_MAX_OPENING) return VALVE_MAX_OPENING;
    if (value < VALVE_MIN_OPENING) return VALVE_MIN_OPENING;
    return value;
}

// 檢查閥門響應
static bool check_valve_response(float command_opening, float actual_opening) {
    float difference = fabsf(actual_opening - command_opening);
    return (difference <= VALVE_RESPONSE_TOLERANCE);
}

// 處理閥門故障
static void handle_valve_fault(valve_controller_t* controller) {
    controller->status.fault_count++;
    controller->status.valve_fault = true;
    
    error(tag, "Valve fault detected (count: %d)", controller->status.fault_count);
    
    if (controller->status.fault_count >= 3) {
        // 連續故障3次，切換到手動模式並設定安全開度
        warn(tag, "Multiple valve faults - switching to manual mode");
        write_holding_register(REG_VALVE_MANUAL, 1);
        write_valve_command(50.0f); // 設定50%安全開度
    }
}

// 緊急流量保護
static float emergency_flow_protection(float current_opening, float actual_flow, 
                                     float high_limit, float low_limit) {
    if (actual_flow > high_limit) {
        // 流量過高，緊急減小開度
        float new_opening = current_opening - EMERGENCY_REDUCTION;
        warn(tag, "Emergency flow reduction: %.1f -> %.1f (flow: %.1f > %.1f)", 
                current_opening, new_opening, actual_flow, high_limit);
        return apply_safety_limits(new_opening);
    }
    
    if (actual_flow < low_limit) {
        // 流量過低，緊急增加開度
        float new_opening = current_opening + EMERGENCY_INCREASE;
        warn(tag, "Emergency flow increase: %.1f -> %.1f (flow: %.1f < %.1f)", 
                current_opening, new_opening, actual_flow, low_limit);
        return apply_safety_limits(new_opening);
    }
    
    return current_opening; // 正常範圍，不調整
}

// ========================================================================================
// 控制邏輯實現
// ========================================================================================

// 手動控制模式
static void execute_manual_control(valve_controller_t* controller) {
    // 手動模式下，外部直接設定 REG_VALVE_COMMAND 寄存器
    // 這裡只進行安全檢查和監控
    
    valve_status_t* status = &controller->status;
    
    // 檢查閥門響應
    if (!check_valve_response(status->command_opening, status->current_opening)) {
        handle_valve_fault(controller);
    } else {
        // 閥門正常，清除故障計數
        if (status->fault_count > 0) {
            status->fault_count = 0;
            status->valve_fault = false;
            info(tag, "Valve response restored");
        }
    }
    
    debug(tag, "Manual mode: Command=%.1f%%, Actual=%.1f%%, Flow=%.1fL/min", 
          status->command_opening, status->current_opening, status->actual_flow);
}

// 自動流量控制模式
static void execute_flow_control(valve_controller_t* controller, uint32_t current_time_ms) {
    valve_config_t* config = &controller->config;
    valve_status_t* status = &controller->status;
    pid_controller_t* flow_pid = &controller->flow_pid;
    
    // 設定PID目標值
    flow_pid->setpoint = config->target_flow;
    flow_pid->enabled = true;
    
    // 緊急流量保護檢查
    float emergency_opening = emergency_flow_protection(
        status->current_opening, 
        status->actual_flow,
        config->flow_high_limit, 
        config->flow_low_limit
    );
    
    if (emergency_opening != status->current_opening) {
        // 緊急情況，直接設定開度
        write_valve_command(emergency_opening);
        return;
    }
    
    // PID控制計算
    float pid_output = pid_calculate(flow_pid, status->actual_flow, current_time_ms);
    
    // 計算新的閥門開度
    float new_opening = status->current_opening + pid_output;
    
    // 限制單次調整量
    float max_adjustment = VALVE_MAX_ADJUSTMENT;
    if (pid_output > max_adjustment) pid_output = max_adjustment;
    if (pid_output < -max_adjustment) pid_output = -max_adjustment;
    
    new_opening = status->current_opening + pid_output;
    new_opening = apply_safety_limits(new_opening);
    
    // 寫入新的命令開度
    if (write_valve_command(new_opening)) {
        info(tag, "Flow Control: Target=%.1f, Actual=%.1f L/min, PID=%.2f, Opening: %.1f -> %.1f%%",
             config->target_flow, status->actual_flow, pid_output,
             status->current_opening, new_opening);
    }
    
    // 檢查閥門響應
    if (!check_valve_response(status->command_opening, status->current_opening)) {
        handle_valve_fault(controller);
    }
}

// ========================================================================================
// 主要函數
// ========================================================================================

// register list init
static int _register_list_init(void)
{
    int ret = SUCCESS;

    _control_logic_register_list[0].name = REG_CONTROL_LOGIC_6_ENABLE_STR;
    _control_logic_register_list[0].address_ptr = &REG_CONTROL_LOGIC_6_ENABLE;
    _control_logic_register_list[0].default_address = REG_CONTROL_LOGIC_6_ENABLE;
    _control_logic_register_list[0].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[1].name = REG_VALVE_ACTUAL_STR;
    _control_logic_register_list[1].address_ptr = &REG_VALVE_STATE;
    _control_logic_register_list[1].default_address = REG_VALVE_STATE;
    _control_logic_register_list[1].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[2].name = REG_VALVE_SETPOINT_STR;
    _control_logic_register_list[2].address_ptr = &REG_VALVE_COMMAND;
    _control_logic_register_list[2].default_address = REG_VALVE_COMMAND;
    _control_logic_register_list[2].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[3].name = REG_VALVE_MANUAL_MODE_STR;
    _control_logic_register_list[3].address_ptr = &REG_VALVE_MANUAL;
    _control_logic_register_list[3].default_address = REG_VALVE_MANUAL;
    _control_logic_register_list[3].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[4].name = REG_FLOW_SETPOINT_STR;
    _control_logic_register_list[4].address_ptr = &REG_FLOW_SETPOINT;
    _control_logic_register_list[4].default_address = REG_FLOW_SETPOINT;
    _control_logic_register_list[4].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[5].name = REG_FLOW_MODE_STR;
    _control_logic_register_list[5].address_ptr = &REG_CONTROL_MODE;
    _control_logic_register_list[5].default_address = REG_CONTROL_MODE;
    _control_logic_register_list[5].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[6].name = REG_FLOW_HIGH_LIMIT_STR;
    _control_logic_register_list[6].address_ptr = &REG_FLOW_HIGH_LIMIT;
    _control_logic_register_list[6].default_address = REG_FLOW_HIGH_LIMIT;
    _control_logic_register_list[6].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[7].name = REG_FLOW_LOW_LIMIT_STR;
    _control_logic_register_list[7].address_ptr = &REG_FLOW_LOW_LIMIT;
    _control_logic_register_list[7].default_address = REG_FLOW_LOW_LIMIT;
    _control_logic_register_list[7].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[8].name = REG_F2_FLOW_STR;
    _control_logic_register_list[8].address_ptr = &REG_F2_FLOW;
    _control_logic_register_list[8].default_address = REG_F2_FLOW;
    _control_logic_register_list[8].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[9].name = REG_P11_PRESSURE_STR;
    _control_logic_register_list[9].address_ptr = &REG_P11_PRESSURE;
    _control_logic_register_list[9].default_address = REG_P11_PRESSURE;
    _control_logic_register_list[9].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[10].name = REG_P17_PRESSURE_STR;
    _control_logic_register_list[10].address_ptr = &REG_P17_PRESSURE;
    _control_logic_register_list[10].default_address = REG_P17_PRESSURE;
    _control_logic_register_list[10].type = CONTROL_LOGIC_REGISTER_READ;

    // try to load register array from file
    uint32_t list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    ret = control_logic_register_load_from_file(CONFIG_REGISTER_FILE_PATH, _control_logic_register_list, list_size);
    debug(tag, "load register array from file %s, ret %d", CONFIG_REGISTER_FILE_PATH, ret);

    return ret;
}

int control_logic_lx1400_6_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

// 初始化函數
int control_logic_lx1400_6_valve_control_init(void) {
    info(tag, "Initializing proportional valve controller");
    // register list init
    _register_list_init();

    // 初始化控制器
    memset(&_valve_controller, 0, sizeof(_valve_controller));
    
    // 初始化PID控制器
    pid_init(&_valve_controller.flow_pid, FLOW_PID_KP, FLOW_PID_KI, FLOW_PID_KD);
    
    // 設定預設配置
    _valve_controller.config.target_flow = 100.0f;        // 預設目標流量
    _valve_controller.config.flow_high_limit = 200.0f;    // 預設流量上限
    _valve_controller.config.flow_low_limit = 50.0f;      // 預設流量下限
    
    _valve_controller.system_initialized = true;
    
    info(tag, "Proportional valve controller initialized successfully");
    return 0;
}

// 主控制函數 - 整合到 control_logic_X 框架
int control_logic_lx1400_6_valve_control(ControlLogic *ptr) {
    if (ptr == NULL) return -1;
    
    // check enable
    if (read_holding_register(REG_CONTROL_LOGIC_6_ENABLE) != 1) {
        return 0;
    }

    uint32_t current_time_ms = time32_get_current_ms();
    
    debug(tag, "Valve control cycle %u", current_time_ms);
    
    // 讀取控制配置
    if (!read_valve_config(&_valve_controller.config)) {
        error(tag, "Failed to read valve configuration");
        _valve_controller.comm_error_count++;
        return -1;
    }
    
    // 讀取閥門狀態
    if (!read_valve_status(&_valve_controller.status)) {
        error(tag, "Failed to read valve status");
        _valve_controller.comm_error_count++;
        return -1;
    }
    
    // 根據控制模式執行對應邏輯
    if (_valve_controller.config.manual_mode) {
        // 手動模式
        execute_manual_control(&_valve_controller);
    } else {
        // 自動流量控制模式
        execute_flow_control(&_valve_controller, current_time_ms);
    }
    
    // 更新統計
    _valve_controller.cycle_count++;
    
    return 0;
}
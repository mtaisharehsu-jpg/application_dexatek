/*
 * control_logic_lx1400_4.c - LX1400 AC泵浦控制邏輯 (Control Logic 4: Pump Control)
 *
 * 【功能概述】
 * 本模組實現 LX1400 機型的 AC 泵浦控制功能,管理3個AC泵浦的啟停、轉速和故障處理。
 * 使用雙 PID 演算法（流量控制 + 壓差控制）協調泵浦運行。
 *
 * 【控制目標】
 * - 流量控制模式: 維持設定流量
 * - 壓差控制模式: 維持設定壓差
 *
 * 【感測器配置】
 * - F1/F2/F3/F4: 流量感測器
 * - P15/P16/P17/P18: 壓力感測器
 * - 泵浦回饋: 頻率/電流/電壓
 *
 * 【執行器控制】
 * - AC Pump1/2/3: 速度設定 (0-1000, 對應0-100%)
 * - 啟停控制: 0=停止, 1=運轉
 * - 故障復歸: 1=復歸
 *
 * 【泵浦協調策略】
 * 根據 PID 輸出決定泵浦數量和速度分配
 *
 * 【雙PID參數】
 * - 流量PID: Kp=2.0, Ki=0.1, Kd=0.5
 * - 壓差PID: Kp=1.5, Ki=0.08, Kd=0.3
 *
 * 【安全保護】
 * - 速度範圍限制
 * - 變化率限制
 * - 故障檢測與復歸
 * - 電流/電壓/功率監測
 *
 * 作者: LX1400 機型適配
 * 日期: 2025
 */

#include "dexatek/main_application/include/application_common.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"

// LX1400T 泵浦控制程式

/* unused function
pid_reset
apply_speed_ramp
REG_TEMP_SETPOINT
REG_CONTROL_MODE
REG_AUTO_START_STOP
REG_PUMP2_MANUAL           
REG_PUMP3_MANUAL
REG_PUMP1_STOP
REG_PUMP2_STOP
REG_PUMP3_STOP
REG_PUMP_MIN_SPEED
REG_PUMP_MAX_SPEED
REG_F1_FLOW
REG_F3_FLOW
REG_F4_FLOW
REG_PUMP1_RESET_CMD
REG_PUMP2_RESET_CMD
REG_PUMP3_RESET_CMD
REG_TEMP_SETPOINT
sys->system_running
sys->emergency_stop
*/

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
static const char* tag = "lx1400_4_pump";


#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_lx1400_4.json"
#define CONFIG_REGISTER_LIST_SIZE 45
static control_logic_register_t _control_logic_register_list[CONFIG_REGISTER_LIST_SIZE];

#define CONTROL_LOGIC_VALUE_CHECK_ENABLE 0

// 系統狀態寄存器
static uint32_t REG_CONTROL_LOGIC_4_ENABLE = 41004; // 控制邏輯4啟用

static uint32_t REG_SYSTEM_STATUS = 42001;    // 機組狀態 bit0:電源 bit1:運轉 bit2:待機 bit7:異常 bit8:液位

// 流量監控寄存器 (讀取)
static uint32_t REG_F1_FLOW = 42062;    // F1一次側進水流量 (0.1LPM)
static uint32_t REG_F2_FLOW = 42063;    // F2二次側出水流量 (0.1LPM)
static uint32_t REG_F3_FLOW = 42064;   // F3二次側進水流量 (0.1LPM)
static uint32_t REG_F4_FLOW = 42065;   // F4一次側出水流量 (0.1LPM)

// 壓力監控寄存器 (讀取)
static uint32_t REG_P15_PRESSURE = 42096;   // P15二次側泵前壓力 (bar)
static uint32_t REG_P16_PRESSURE = 42097;   // P16二次側泵前壓力 (bar)
static uint32_t REG_P17_PRESSURE = 42098;   // P17二次側出水壓力 (bar)
static uint32_t REG_P18_PRESSURE = 42099;   // P18二次側出水壓力 (bar)

// 泵浦回饋寄存器 (讀取)
static uint32_t REG_PUMP1_FREQ = 42501;    // AC泵浦1輸出頻率 (Hz)
static uint32_t REG_PUMP2_FREQ = 42511;    // AC泵浦2輸出頻率 (Hz)
static uint32_t REG_PUMP3_FREQ = 42521;    // AC泵浦3輸出頻率 (Hz)
static uint32_t REG_PUMP1_CURRENT = 42502;    // AC泵浦1電流 (A×0.01)
static uint32_t REG_PUMP2_CURRENT = 42512;    // AC泵浦2電流 (A×0.01)
static uint32_t REG_PUMP3_CURRENT = 42522;    // AC泵浦3電流 (A×0.01)
static uint32_t REG_PUMP1_VOLTAGE = 42503;    // AC泵浦1電壓 (V×0.1)
static uint32_t REG_PUMP2_VOLTAGE = 42513;    // AC泵浦2電壓 (V×0.1)
static uint32_t REG_PUMP3_VOLTAGE = 42523;    // AC泵浦3電壓 (V×0.1)

// 泵浦控制寄存器 (寫入)
static uint32_t REG_PUMP1_SPEED_CMD = 411037;   // AC泵浦1設定轉速 (0-1000, 對應0-100%)
static uint32_t REG_PUMP2_SPEED_CMD = 411039;   // AC泵浦2設定轉速 (0-1000, 對應0-100%)
static uint32_t REG_PUMP3_SPEED_CMD = 411041;   // AC泵浦3設定轉速 (0-1000, 對應0-100%)
static uint32_t REG_PUMP1_RUN_CMD = 411101;   // 泵浦1啟停控制 (0=停止, 1=運轉)
static uint32_t REG_PUMP2_RUN_CMD = 411103;   // 泵浦2啟停控制 (0=停止, 1=運轉)
static uint32_t REG_PUMP3_RUN_CMD = 411105;   // 泵浦3啟停控制 (0=停止, 1=運轉)
static uint32_t REG_PUMP1_RESET_CMD = 411102;   // 泵浦1異常復歸 (1=復歸)
static uint32_t REG_PUMP2_RESET_CMD = 411104;   // 泵浦2異常復歸 (1=復歸)
static uint32_t REG_PUMP3_RESET_CMD = 411106;   // 泵浦3異常復歸 (1=復歸)

// 泵浦狀態輸入寄存器 (讀取)
static uint32_t REG_PUMP1_FAULT = 411109;   // 泵浦1過載狀態 (0=故障)
static uint32_t REG_PUMP2_FAULT = 411110;   // 泵浦2過載狀態 (0=故障)
static uint32_t REG_PUMP3_FAULT = 411111;   // 泵浦3過載狀態 (0=故障)

// 控制設定值寄存器
static uint32_t REG_TEMP_SETPOINT = 45001;    // 目標控制溫度 (°C)
static uint32_t REG_PRESSURE_SETPOINT = 45002;    // 目標控制壓差 (bar)
static uint32_t REG_FLOW_SETPOINT = 45003;    // 目標控制流量 (L/min)
static uint32_t REG_CONTROL_MODE = 45005;    // 流量模式/壓差模式 (0/1)
static uint32_t REG_AUTO_START_STOP = 45020;    // 自動啟動/停止 (0/1)

// 手動模式控制寄存器
static uint32_t REG_PUMP1_MANUAL = 45021;    // 泵浦1手動模式 (0/1)
static uint32_t REG_PUMP2_MANUAL = 45022;    // 泵浦2手動模式 (0/1)
static uint32_t REG_PUMP3_MANUAL = 45023;    // 泵浦3手動模式 (0/1)
static uint32_t REG_PUMP1_STOP = 45025;    // 泵浦1停用 (0/1)
static uint32_t REG_PUMP2_STOP = 45026;    // 泵浦2停用 (0/1)
static uint32_t REG_PUMP3_STOP = 45027;    // 泵浦3停用 (0/1)
static uint32_t REG_PUMP_MIN_SPEED = 45031;    // 泵浦最小速度 (%)
static uint32_t REG_PUMP_MAX_SPEED = 45032;    // 泵浦最大速度 (%)

#define MAX_PUMPS                   3
#define CONTROL_CYCLE_MS            200      // 5Hz控制週期
#define PUMP_SPEED_MIN              10.0f    // 最小泵浦轉速 (%)
#define PUMP_SPEED_MAX              100.0f   // 最大泵浦轉速 (%)
#define PUMP_SPEED_RAMP_RATE        10.0f    // 轉速變化率 (%/s)
#define PUMP_SPEED_SCALE            10.0f    // 轉速換算係數 (0-100% → 0-1000)

// PID控制參數
#define PID_OUTPUT_MIN              -50.0f
#define PID_OUTPUT_MAX              50.0f
#define PID_INTEGRAL_MAX            1000.0f

// 控制模式
typedef enum {
    CONTROL_MODE_MANUAL = 0,       // 手動模式
    CONTROL_MODE_FLOW,             // 流量控制模式
    CONTROL_MODE_PRESSURE          // 壓差控制模式
} control_mode_t;

// PID控制器
typedef struct {
    float kp, ki, kd;              // PID參數
    float setpoint;                // 設定值
    float integral;                // 積分項
    float last_error;              // 上次誤差
    float output;                  // 輸出
    float output_min, output_max;  // 輸出限制
    bool enabled;                  // 是否啟用
} pid_controller_t;

// 單台泵浦狀態
typedef struct {
    uint8_t pump_id;               // 泵浦編號 (1-3)
    bool running;                  // 是否運轉
    bool enabled;                  // 是否啟用
    bool fault;                    // 故障狀態
    bool manual_mode;              // 手動模式
    float speed_setpoint;          // 轉速設定值 (%)
    float speed_feedback;          // 轉速回饋值 (Hz)
    float speed_command;           // 最終輸出指令 (%)
    float current;                 // 電流值 (A)
    float voltage;                 // 電壓值 (V)
    uint32_t start_count;          // 啟動次數
    uint32_t last_start_time;      // 最後啟動時間
} pump_t;

// 系統數據
typedef struct {
    // 流量數據
    float flow_current;            // 當前流量 (L/min)
    float flow_setpoint;           // 流量設定值 (L/min)
    
    // 壓力數據
    float inlet_pressure;          // 入口壓力 (bar)
    float outlet_pressure;         // 出口壓力 (bar)
    float pressure_diff;           // 壓差 (bar)
    float pressure_setpoint;       // 壓差設定值 (bar)
    
    // 系統狀態
    bool system_running;           // 系統運轉狀態
    bool emergency_stop;           // 緊急停止
    bool modbus_connected;         // Modbus連線狀態
    uint32_t comm_error_count;     // 通信錯誤計數
    
} system_data_t;

// 主控制器結構
typedef struct {
    // 控制模式與參數
    control_mode_t control_mode;
    bool auto_start_enabled;       // 自動啟動功能
    
    // 泵浦陣列
    pump_t pumps[MAX_PUMPS];
    uint8_t active_pump_count;     // 活動泵浦數量
    uint8_t lead_pump;             // 主泵編號 (0-2)
    
    // PID控制器
    pid_controller_t flow_pid;
    pid_controller_t pressure_pid;
    
    // 系統數據
    system_data_t system;
    
    // 統計數據
    uint32_t cycle_count;
    uint32_t max_cycle_time;
    
} simple_pump_controller_t;

/*---------------------------------------------------------------------------
								Variables
 ---------------------------------------------------------------------------*/
static simple_pump_controller_t _controller = {0};
static bool manual_mode_enable = false;

/*---------------------------------------------------------------------------
							Function Prototypes
 ---------------------------------------------------------------------------*/
// PID控制函數
static void pid_init(pid_controller_t* pid, float kp, float ki, float kd);
static float pid_calculate(pid_controller_t* pid, float process_value, float dt);
// static void pid_reset(pid_controller_t* pid);

// 系統數據讀取
static bool read_system_data(simple_pump_controller_t* controller);

// 泵浦控制函數
static void set_pump_speed_command(uint8_t pump_id, float speed);
static void set_pump_run_command(uint8_t pump_id, bool run);
// static float apply_speed_ramp(float current_speed, float target_speed, float dt);

// 控制邏輯函數
static void execute_manual_control(simple_pump_controller_t* controller);
static void execute_flow_control(simple_pump_controller_t* controller, float dt);
static void execute_pressure_control(simple_pump_controller_t* controller, float dt);

// Modbus通信函數 (透過 control_hardware 實現)
static uint16_t read_holding_register(uint32_t address);
static bool write_holding_register(uint32_t address, uint16_t value);

/*---------------------------------------------------------------------------
                                Implementation
 ---------------------------------------------------------------------------*/

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
    pid->enabled = false;
}

// PID計算
static float pid_calculate(pid_controller_t* pid, float process_value, float dt) {
    if (!pid->enabled || dt <= 0.0f) {
        return 0.0f;
    }
    
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
    
    return pid->output;
}

// 重置PID控制器
// static void pid_reset(pid_controller_t* pid) {
//     pid->integral = 0.0f;
//     pid->last_error = 0.0f;
//     pid->output = 0.0f;
// }

// ========================================================================================
// Modbus通信實現 (透過 control_hardware 模組)
// ========================================================================================

static uint16_t read_holding_register(uint32_t address) {    
    uint16_t value = 0;
    int ret = control_logic_read_holding_register(address, &value);

    if (ret != SUCCESS) {
        value = 0xFFFF;
    }

    return value;
}

static bool write_holding_register(uint32_t address, uint16_t value) {
   
    int ret = control_logic_write_register(address, value, 2000);

    return (ret == SUCCESS)? true : false;
}

// ========================================================================================
// 系統數據讀取
// ========================================================================================

// 讀取系統數據
static bool read_system_data(simple_pump_controller_t* controller) {
    system_data_t* sys = &controller->system;
    bool success = true;
    
    // 讀取流量數據
    uint16_t flow_raw = read_holding_register(REG_F2_FLOW);
    // info(tag, "flow_raw=%d (485)(%d)", flow_raw, REG_F2_FLOW);

    if (flow_raw != 0xFFFF) {
        sys->flow_current = (float)flow_raw / 10.0f; // 0.1 LPM換算
    } else {
        success = false;
    }
    
    // 讀取壓力數據
    uint16_t p15 = read_holding_register(REG_P15_PRESSURE);
    uint16_t p16 = read_holding_register(REG_P16_PRESSURE);
    uint16_t p17 = read_holding_register(REG_P17_PRESSURE);
    uint16_t p18 = read_holding_register(REG_P18_PRESSURE);
    // info(tag, "p15=%d (485)(%d)", p15, REG_P15_PRESSURE);
    // info(tag, "p16=%d (485)(%d)", p16, REG_P16_PRESSURE);
    // info(tag, "p17=%d (485)(%d)", p17, REG_P17_PRESSURE);
    // info(tag, "p18=%d (485)(%d)", p18, REG_P18_PRESSURE);

    if (p15 != 0xFFFF && p16 != 0xFFFF && p17 != 0xFFFF && p18 != 0xFFFF) {
        // 計算平均壓力 (依據定義表，直接為bar單位)
        sys->inlet_pressure = ((float)p15 + (float)p16) / 2.0f;
        sys->outlet_pressure = ((float)p17 + (float)p18) / 2.0f;
        sys->pressure_diff = sys->outlet_pressure - sys->inlet_pressure;
    } else {
        success = false;
    }
    
    // 讀取泵浦回饋數據
    for (int i = 0; i < MAX_PUMPS; i++) {
        // 讀取頻率回饋
        uint16_t freq_raw = read_holding_register(REG_PUMP1_FREQ + (i * 10));
        // info(tag, "freq_raw[%d]=%d (485)(%d)", i, freq_raw, REG_PUMP1_FREQ + (i * 10));
        if (freq_raw != 0xFFFF) {
            controller->pumps[i].speed_feedback = (float)freq_raw; // Hz
        } else {
            success = false;
        }
        
        // 讀取電流
        uint16_t current_raw = read_holding_register(REG_PUMP1_CURRENT + (i * 10));
        // info(tag, "current_raw[%d]=%d (485)(%d)", i, current_raw, REG_PUMP1_CURRENT + (i * 10));
        if (current_raw != 0xFFFF) {
            controller->pumps[i].current = (float)current_raw / 100.0f; // A×0.01換算
        }
        
        // 讀取電壓
        uint16_t voltage_raw = read_holding_register(REG_PUMP1_VOLTAGE + (i * 10));
        // info(tag, "voltage_raw[%d]=%d (485)(%d)", i, voltage_raw, REG_PUMP1_VOLTAGE + (i * 10));
        if (voltage_raw != 0xFFFF) {
            controller->pumps[i].voltage = (float)voltage_raw / 10.0f; // V×0.1換算
        }
        
        // 讀取故障狀態
        uint16_t fault_raw = read_holding_register(REG_PUMP1_FAULT + i);
        // info(tag, "fault_raw[%d]=%d (DI_%d)(%d)", i, fault_raw, i, REG_PUMP1_FAULT + i);
        controller->pumps[i].fault = (fault_raw == 0); // 0=故障
    }
    
    // 讀取系統狀態
    uint16_t system_status = read_holding_register(REG_SYSTEM_STATUS);
    // info(tag, "system_status=%d (HMI)(%d)", system_status, REG_SYSTEM_STATUS);
    if (system_status != 0xFFFF) {
        sys->system_running = (system_status & 0x02) != 0; // bit1: 運轉
        sys->emergency_stop = (system_status & 0x80) != 0; // bit7: 異常
    } else {
        success = false;
    }
    
    if (!success) {
        sys->comm_error_count++;
    }
    
    sys->modbus_connected = success;
    return success;
}

// ========================================================================================
// 泵浦控制函數
// ========================================================================================

// 設定泵浦轉速指令
static void set_pump_speed_command(uint8_t pump_id, float speed) {
#if defined(CONTROL_LOGIC_VALUE_CHECK_ENABLE) && CONTROL_LOGIC_VALUE_CHECK_ENABLE == 1
    static uint32_t _speed_cmd_last[MAX_PUMPS] = {-1};
#endif

    if (pump_id >= MAX_PUMPS) return;
    
    // 轉速限制檢查
    if (speed > PUMP_SPEED_MAX) speed = PUMP_SPEED_MAX;
    if (speed < 0.0f) speed = 0.0f;
    
    // 如果轉速小於最小值但大於0，則設為0（停止泵浦）
    if (speed > 0.0f && speed < PUMP_SPEED_MIN) {
        speed = 0.0f;
    }

    // 轉換為寄存器值 (0-100% → 0-1000)
    uint16_t speed_cmd = (uint16_t)(speed * PUMP_SPEED_SCALE);
    uint32_t reg_addr = REG_PUMP1_SPEED_CMD + (pump_id * 2); // 泵浦1:411037, 泵浦2:411039, 泵浦3:411041
    
    // [DK]: 1000 means 10V (10000mV) => speed_cmd * 10 
    speed_cmd = speed_cmd * 10; // [DK]

    // debug(tag, "Pump %d speed: %f, speed_cmd: %d", pump_id + 1, speed, speed_cmd);

#if defined(CONTROL_LOGIC_VALUE_CHECK_ENABLE) && CONTROL_LOGIC_VALUE_CHECK_ENABLE == 1
    if (_speed_cmd_last[pump_id] != speed_cmd) {
        bool write_ret = write_holding_register(reg_addr, speed_cmd);
        if (write_ret == true) {
            _speed_cmd_last[pump_id] = speed_cmd;
        }
    }
#else
    write_holding_register(reg_addr, speed_cmd);
#endif
    
    debug(tag, "Pump %d speed command: %d (AO_%d)(%d)", pump_id + 1, speed_cmd, pump_id, reg_addr);
}

// 設定泵浦啟停指令
static void set_pump_run_command(uint8_t pump_id, bool run) {
#if defined(CONTROL_LOGIC_VALUE_CHECK_ENABLE) && CONTROL_LOGIC_VALUE_CHECK_ENABLE == 1
    static uint32_t _run_cmd_last[MAX_PUMPS] = {-1};
#endif
    if (pump_id >= MAX_PUMPS) return;
    
    uint16_t run_cmd = run ? 1 : 0;
    uint32_t reg_addr = REG_PUMP1_RUN_CMD + (pump_id * 2); // 泵浦1:411101, 泵浦2:411103, 泵浦3:411105
    
#if defined(CONTROL_LOGIC_VALUE_CHECK_ENABLE) && CONTROL_LOGIC_VALUE_CHECK_ENABLE == 1
    if (_run_cmd_last[pump_id] != run_cmd) {
        bool write_ret = write_holding_register(reg_addr, run_cmd);
        if (write_ret == true) {
            _run_cmd_last[pump_id] = run_cmd;
        }
    }
#else
    write_holding_register(reg_addr, run_cmd);
#endif

    debug(tag, "Pump %d run command: %s (D0_%d)(%d)", pump_id + 1, run ? "RUN" : "STOP", pump_id,reg_addr);
}

// 漸變調整泵浦轉速變化
// static float apply_speed_ramp(float current_speed, float target_speed, float dt) {
//     float max_change = PUMP_SPEED_RAMP_RATE * dt;
//     float speed_diff = target_speed - current_speed;
    
//     if (fabsf(speed_diff) <= max_change) {
//         return target_speed;
//     } else if (speed_diff > 0) {
//         return current_speed + max_change;
//     } else {
//         return current_speed - max_change;
//     }
// }

// ========================================================================================
// 控制邏輯實現
// ========================================================================================

// 手動控制模式
static void execute_manual_control(simple_pump_controller_t* controller) {
    for (int i = 0; i < MAX_PUMPS; i++) {
        pump_t* pump = &controller->pumps[i];
        
        if (!pump->enabled || pump->fault) {
            set_pump_speed_command(i, 0.0f);
            set_pump_run_command(i, false);
            continue;
        }
        
        // 直接使用設定值（已含漸變速度控制）
        set_pump_speed_command(i, pump->speed_setpoint);
        set_pump_run_command(i, pump->speed_setpoint > 0.0f);
    }
    
    debug(tag, "Manual control mode executed");
}

// 流量控制模式
static void execute_flow_control(simple_pump_controller_t* controller, float dt) {
    if (!controller->flow_pid.enabled) return;
    
    // 計算PID輸出
    float pid_output = pid_calculate(&controller->flow_pid, 
                                    controller->system.flow_current, dt);
    
    // 確定主泵
    uint8_t lead_pump = controller->lead_pump;
    if (!controller->pumps[lead_pump].enabled || controller->pumps[lead_pump].fault) {
        // 主泵不可用，尋找可用泵浦
        for (int i = 0; i < MAX_PUMPS; i++) {
            if (controller->pumps[i].enabled && !controller->pumps[i].fault) {
                lead_pump = i;
                controller->lead_pump = i;
                break;
            }
        }
    }
    
    // 計算新泵浦轉速設定值
    float base_speed = 50.0f; // 基礎轉速
    float new_speed = base_speed + pid_output;
    
    // 轉速限制檢查
    if (new_speed > PUMP_SPEED_MAX) new_speed = PUMP_SPEED_MAX;
    if (new_speed < PUMP_SPEED_MIN && new_speed > 0.0f) new_speed = PUMP_SPEED_MIN;
    if (new_speed < 0.0f) new_speed = 0.0f;
    
    // 控制主泵
    controller->pumps[lead_pump].speed_setpoint = new_speed;
    set_pump_speed_command(lead_pump, new_speed);
    set_pump_run_command(lead_pump, new_speed > 0.0f);
    
    // 其他泵浦停止
    for (int i = 0; i < MAX_PUMPS; i++) {
        if (i != lead_pump) {
            controller->pumps[i].speed_setpoint = 0.0f;
            set_pump_speed_command(i, 0.0f);
            set_pump_run_command(i, false);
        }
    }
    
    // 簡單的多泵切換邏輯
    if (new_speed >= 90.0f && controller->active_pump_count < 2) {
        // 主泵接近滿載，啟動第二台泵
        for (int i = 0; i < MAX_PUMPS; i++) {
            if (i != lead_pump && controller->pumps[i].enabled && !controller->pumps[i].fault) {
                controller->pumps[i].speed_setpoint = 30.0f;
                set_pump_speed_command(i, 30.0f);
                set_pump_run_command(i, true);
                controller->active_pump_count = 2;
                info(tag, "Started secondary pump %d", i + 1);
                break;
            }
        }
    }
    
    info(tag, "Flow Control: Current=%.1f L/min, Setpoint=%.1f L/min, PID=%.1f, Speed=%.1f%%",
           controller->system.flow_current, controller->flow_pid.setpoint, 
           pid_output, new_speed);
}

// 壓差控制模式
static void execute_pressure_control(simple_pump_controller_t* controller, float dt) {
    if (!controller->pressure_pid.enabled) return;
    
    // 計算PID輸出
    float pid_output = pid_calculate(&controller->pressure_pid,
                                    controller->system.pressure_diff, dt);
    
    // 確定主泵
    uint8_t lead_pump = controller->lead_pump;
    if (!controller->pumps[lead_pump].enabled || controller->pumps[lead_pump].fault) {
        for (int i = 0; i < MAX_PUMPS; i++) {
            if (controller->pumps[i].enabled && !controller->pumps[i].fault) {
                lead_pump = i;
                controller->lead_pump = i;
                break;
            }
        }
    }
    
    // 計算新泵浦轉速設定值
    float base_speed = 60.0f; // 基礎轉速
    float new_speed = base_speed + pid_output;
    
    // 轉速限制檢查
    if (new_speed > PUMP_SPEED_MAX) new_speed = PUMP_SPEED_MAX;
    if (new_speed < PUMP_SPEED_MIN && new_speed > 0.0f) new_speed = PUMP_SPEED_MIN;
    if (new_speed < 0.0f) new_speed = 0.0f;
    
    // 控制主泵
    controller->pumps[lead_pump].speed_setpoint = new_speed;
    set_pump_speed_command(lead_pump, new_speed);
    set_pump_run_command(lead_pump, new_speed > 0.0f);
    
    // 其他泵浦停止
    for (int i = 0; i < MAX_PUMPS; i++) {
        if (i != lead_pump) {
            controller->pumps[i].speed_setpoint = 0.0f;
            set_pump_speed_command(i, 0.0f);
            set_pump_run_command(i, false);
        }
    }
    
    info(tag, "Pressure Control: Current=%.2f bar, Setpoint=%.2f bar, PID=%.1f, Speed=%.1f%%",
           controller->system.pressure_diff, controller->pressure_pid.setpoint,
           pid_output, new_speed);
}

static int _register_list_init(void)
{
    int ret = SUCCESS;

    _control_logic_register_list[0].name = REG_CONTROL_LOGIC_4_ENABLE_STR;
    _control_logic_register_list[0].address_ptr = &REG_CONTROL_LOGIC_4_ENABLE;
    _control_logic_register_list[0].default_address = REG_CONTROL_LOGIC_4_ENABLE;
    _control_logic_register_list[0].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[1].name = REG_SYSTEM_STATUS_STR;
    _control_logic_register_list[1].address_ptr = &REG_SYSTEM_STATUS;
    _control_logic_register_list[1].default_address = REG_SYSTEM_STATUS;
    _control_logic_register_list[1].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[2].name = REG_F1_FLOW_STR;
    _control_logic_register_list[2].address_ptr = &REG_F1_FLOW;
    _control_logic_register_list[2].default_address = REG_F1_FLOW;
    _control_logic_register_list[2].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[3].name = REG_F2_FLOW_STR;
    _control_logic_register_list[3].address_ptr = &REG_F2_FLOW;
    _control_logic_register_list[3].default_address = REG_F2_FLOW;
    _control_logic_register_list[3].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[4].name = REG_F3_FLOW_STR;
    _control_logic_register_list[4].address_ptr = &REG_F3_FLOW;
    _control_logic_register_list[4].default_address = REG_F3_FLOW;
    _control_logic_register_list[4].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[5].name = REG_F4_FLOW_STR;
    _control_logic_register_list[5].address_ptr = &REG_F4_FLOW;
    _control_logic_register_list[5].default_address = REG_F4_FLOW;
    _control_logic_register_list[5].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[6].name = REG_P15_PRESSURE_STR;
    _control_logic_register_list[6].address_ptr = &REG_P15_PRESSURE;
    _control_logic_register_list[6].default_address = REG_P15_PRESSURE;
    _control_logic_register_list[6].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[7].name = REG_P16_PRESSURE_STR;
    _control_logic_register_list[7].address_ptr = &REG_P16_PRESSURE;
    _control_logic_register_list[7].default_address = REG_P16_PRESSURE;
    _control_logic_register_list[7].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[8].name = REG_P17_PRESSURE_STR;
    _control_logic_register_list[8].address_ptr = &REG_P17_PRESSURE;
    _control_logic_register_list[8].default_address = REG_P17_PRESSURE;
    _control_logic_register_list[8].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[9].name = REG_P18_PRESSURE_STR;
    _control_logic_register_list[9].address_ptr = &REG_P18_PRESSURE;
    _control_logic_register_list[9].default_address = REG_P18_PRESSURE;
    _control_logic_register_list[9].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[10].name = REG_PUMP1_FREQ_STR;
    _control_logic_register_list[10].address_ptr = &REG_PUMP1_FREQ;
    _control_logic_register_list[10].default_address = REG_PUMP1_FREQ;
    _control_logic_register_list[10].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[11].name = REG_PUMP2_FREQ_STR;
    _control_logic_register_list[11].address_ptr = &REG_PUMP2_FREQ;
    _control_logic_register_list[11].default_address = REG_PUMP2_FREQ;
    _control_logic_register_list[11].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[12].name = REG_PUMP3_FREQ_STR;
    _control_logic_register_list[12].address_ptr = &REG_PUMP3_FREQ;
    _control_logic_register_list[12].default_address = REG_PUMP3_FREQ;
    _control_logic_register_list[12].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[13].name = REG_PUMP1_CURRENT_STR;
    _control_logic_register_list[13].address_ptr = &REG_PUMP1_CURRENT;
    _control_logic_register_list[13].default_address = REG_PUMP1_CURRENT;
    _control_logic_register_list[13].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[14].name = REG_PUMP2_CURRENT_STR;
    _control_logic_register_list[14].address_ptr = &REG_PUMP2_CURRENT;
    _control_logic_register_list[14].default_address = REG_PUMP2_CURRENT;
    _control_logic_register_list[14].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[15].name = REG_PUMP3_CURRENT_STR;
    _control_logic_register_list[15].address_ptr = &REG_PUMP3_CURRENT;
    _control_logic_register_list[15].default_address = REG_PUMP3_CURRENT;
    _control_logic_register_list[15].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[16].name = REG_PUMP1_VOLTAGE_STR;
    _control_logic_register_list[16].address_ptr = &REG_PUMP1_VOLTAGE;
    _control_logic_register_list[16].default_address = REG_PUMP1_VOLTAGE;
    _control_logic_register_list[16].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[17].name = REG_PUMP2_VOLTAGE_STR;
    _control_logic_register_list[17].address_ptr = &REG_PUMP2_VOLTAGE;
    _control_logic_register_list[17].default_address = REG_PUMP2_VOLTAGE;
    _control_logic_register_list[17].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[18].name = REG_PUMP3_VOLTAGE_STR;
    _control_logic_register_list[18].address_ptr = &REG_PUMP3_VOLTAGE;
    _control_logic_register_list[18].default_address = REG_PUMP3_VOLTAGE;
    _control_logic_register_list[18].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[19].name = REG_PUMP1_SPEED_CMD_STR;
    _control_logic_register_list[19].address_ptr = &REG_PUMP1_SPEED_CMD;
    _control_logic_register_list[19].default_address = REG_PUMP1_SPEED_CMD;
    _control_logic_register_list[19].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[20].name = REG_PUMP2_SPEED_CMD_STR;
    _control_logic_register_list[20].address_ptr = &REG_PUMP2_SPEED_CMD;
    _control_logic_register_list[20].default_address = REG_PUMP2_SPEED_CMD;
    _control_logic_register_list[20].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[21].name = REG_PUMP3_SPEED_CMD_STR;
    _control_logic_register_list[21].address_ptr = &REG_PUMP3_SPEED_CMD;
    _control_logic_register_list[21].default_address = REG_PUMP3_SPEED_CMD;
    _control_logic_register_list[21].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[22].name = REG_PUMP1_RUN_CMD_STR;
    _control_logic_register_list[22].address_ptr = &REG_PUMP1_RUN_CMD;
    _control_logic_register_list[22].default_address = REG_PUMP1_RUN_CMD;
    _control_logic_register_list[22].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[23].name = REG_PUMP2_RUN_CMD_STR;
    _control_logic_register_list[23].address_ptr = &REG_PUMP2_RUN_CMD;
    _control_logic_register_list[23].default_address = REG_PUMP2_RUN_CMD;
    _control_logic_register_list[23].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[24].name = REG_PUMP3_RUN_CMD_STR;
    _control_logic_register_list[24].address_ptr = &REG_PUMP3_RUN_CMD;
    _control_logic_register_list[24].default_address = REG_PUMP3_RUN_CMD;
    _control_logic_register_list[24].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[25].name = REG_PUMP1_RESET_CMD_STR;
    _control_logic_register_list[25].address_ptr = &REG_PUMP1_RESET_CMD;
    _control_logic_register_list[25].default_address = REG_PUMP1_RESET_CMD;
    _control_logic_register_list[25].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[26].name = REG_PUMP2_RESET_CMD_STR;
    _control_logic_register_list[26].address_ptr = &REG_PUMP2_RESET_CMD;
    _control_logic_register_list[26].default_address = REG_PUMP2_RESET_CMD;
    _control_logic_register_list[26].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[27].name = REG_PUMP3_RESET_CMD_STR;
    _control_logic_register_list[27].address_ptr = &REG_PUMP3_RESET_CMD;
    _control_logic_register_list[27].default_address = REG_PUMP3_RESET_CMD;
    _control_logic_register_list[27].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[28].name = REG_PUMP1_FAULT_STR;
    _control_logic_register_list[28].address_ptr = &REG_PUMP1_FAULT;
    _control_logic_register_list[28].default_address = REG_PUMP1_FAULT;
    _control_logic_register_list[28].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[29].name = REG_PUMP2_FAULT_STR;
    _control_logic_register_list[29].address_ptr = &REG_PUMP2_FAULT;
    _control_logic_register_list[29].default_address = REG_PUMP2_FAULT;
    _control_logic_register_list[29].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[30].name = REG_PUMP3_FAULT_STR;
    _control_logic_register_list[30].address_ptr = &REG_PUMP3_FAULT;
    _control_logic_register_list[30].default_address = REG_PUMP3_FAULT;
    _control_logic_register_list[30].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[31].name = REG_TARGET_TEMP_STR;
    _control_logic_register_list[31].address_ptr = &REG_TEMP_SETPOINT;
    _control_logic_register_list[31].default_address = REG_TEMP_SETPOINT;
    _control_logic_register_list[31].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[32].name = REG_PRESSURE_SETPOINT_STR;
    _control_logic_register_list[32].address_ptr = &REG_PRESSURE_SETPOINT;
    _control_logic_register_list[32].default_address = REG_PRESSURE_SETPOINT;
    _control_logic_register_list[32].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[33].name = REG_FLOW_SETPOINT_STR;
    _control_logic_register_list[33].address_ptr = &REG_FLOW_SETPOINT;
    _control_logic_register_list[33].default_address = REG_FLOW_SETPOINT;
    _control_logic_register_list[33].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[34].name = REG_FLOW_MODE_STR;
    _control_logic_register_list[34].address_ptr = &REG_CONTROL_MODE;
    _control_logic_register_list[34].default_address = REG_CONTROL_MODE;
    _control_logic_register_list[34].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[35].name = REG_AUTO_START_STOP_STR;
    _control_logic_register_list[35].address_ptr = &REG_AUTO_START_STOP;
    _control_logic_register_list[35].default_address = REG_AUTO_START_STOP;
    _control_logic_register_list[35].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[36].name = REG_PUMP1_MANUAL_MODE_STR;
    _control_logic_register_list[36].address_ptr = &REG_PUMP1_MANUAL;
    _control_logic_register_list[36].default_address = REG_PUMP1_MANUAL;
    _control_logic_register_list[36].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[37].name = REG_PUMP2_MANUAL_MODE_STR;
    _control_logic_register_list[37].address_ptr = &REG_PUMP2_MANUAL;
    _control_logic_register_list[37].default_address = REG_PUMP2_MANUAL;
    _control_logic_register_list[37].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[38].name = REG_PUMP3_MANUAL_MODE_STR;
    _control_logic_register_list[38].address_ptr = &REG_PUMP3_MANUAL;
    _control_logic_register_list[38].default_address = REG_PUMP3_MANUAL;
    _control_logic_register_list[38].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[39].name = REG_PUMP1_STOP_STR;
    _control_logic_register_list[39].address_ptr = &REG_PUMP1_STOP;
    _control_logic_register_list[39].default_address = REG_PUMP1_STOP;
    _control_logic_register_list[39].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[40].name = REG_PUMP2_STOP_STR;
    _control_logic_register_list[40].address_ptr = &REG_PUMP2_STOP;
    _control_logic_register_list[40].default_address = REG_PUMP2_STOP;
    _control_logic_register_list[40].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[41].name = REG_PUMP3_STOP_STR;
    _control_logic_register_list[41].address_ptr = &REG_PUMP3_STOP;
    _control_logic_register_list[41].default_address = REG_PUMP3_STOP;
    _control_logic_register_list[41].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[42].name = REG_PUMP_MIN_SPEED_STR;
    _control_logic_register_list[42].address_ptr = &REG_PUMP_MIN_SPEED;
    _control_logic_register_list[42].default_address = REG_PUMP_MIN_SPEED;
    _control_logic_register_list[42].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[43].name = REG_PUMP_MAX_SPEED_STR;
    _control_logic_register_list[43].address_ptr = &REG_PUMP_MAX_SPEED;
    _control_logic_register_list[43].default_address = REG_PUMP_MAX_SPEED;
    _control_logic_register_list[43].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // try to load register array from file
    uint32_t list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    ret = control_logic_register_load_from_file(CONFIG_REGISTER_FILE_PATH, _control_logic_register_list, list_size);
    debug(tag, "load register array from file %s, ret %d", CONFIG_REGISTER_FILE_PATH, ret);

    return ret;
}

int control_logic_lx1400_4_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

int control_logic_lx1400_4_pump_control_init(void) 
{
    info(tag, "Initializing LX1400T pump controller");    // register list init
    _register_list_init();
    
    // 初始化控制器
    memset(&_controller, 0, sizeof(_controller));
    
    // 初始化泵浦
    for (int i = 0; i < MAX_PUMPS; i++) {
        _controller.pumps[i].pump_id = i + 1;
        _controller.pumps[i].enabled = true;
        _controller.pumps[i].manual_mode = false;
    }
    
    // 初始化PID控制器
    pid_init(&_controller.flow_pid, 2.0f, 0.1f, 0.5f);
    pid_init(&_controller.pressure_pid, 1.5f, 0.08f, 0.3f);
    
    // 設定預設參數
    _controller.control_mode = CONTROL_MODE_FLOW;
    _controller.lead_pump = 0;
    _controller.active_pump_count = 1;
    _controller.auto_start_enabled = true;
    
    info(tag, "LX1400T pump controller initialized successfully");
    return 0;
}

int control_logic_lx1400_4_pump_control(ControlLogic *ptr)
{
    (void)ptr;
    // debug(tag, "Control logic 3 %u", ptr->latest_timestamp_ms);

    // check enable
    if (read_holding_register(REG_CONTROL_LOGIC_4_ENABLE) != 1) {
        return 0;
    }
    
    // 計算時間差
    uint32_t current_timestamp_ms = time32_get_current_ms();
    float dt = (current_timestamp_ms - ptr->latest_timestamp_ms) / 1000.0f;
    
    debug(tag, "Control logic 3 cycle %u, dt=%.3f", ptr->latest_timestamp_ms, dt);
    
    // 讀取系統數據
    if (!read_system_data(&_controller)) {
        error(tag, "Failed to read system data");
        return -1;
    }
    
    // 檢查手動模式/*  */
    uint16_t manual_mode_reg = read_holding_register(REG_PUMP1_MANUAL);
    manual_mode_enable = (manual_mode_reg != 0);
    debug(tag, "manual_mode_enable = %d (HMI)(%d)", manual_mode_enable, REG_PUMP1_MANUAL);
    
    // 執行控制邏輯
    if (manual_mode_enable) {
        // 手動模式，由外部設定各泵浦參數
        execute_manual_control(&_controller);
    } else {
        // 自動模式
        if (_controller.control_mode == CONTROL_MODE_FLOW) {
            // 讀取流量設定值
            uint16_t flow_sp_raw = read_holding_register(REG_FLOW_SETPOINT);
            debug(tag, "flow_sp_raw=%d (HMI)(%d)", flow_sp_raw, REG_FLOW_SETPOINT);
            if (flow_sp_raw != 0xFFFF) {
                _controller.flow_pid.setpoint = (float)flow_sp_raw / 10.0f;
                _controller.flow_pid.enabled = true;
            }
            execute_flow_control(&_controller, dt);
        } else if (_controller.control_mode == CONTROL_MODE_PRESSURE) {
            // 讀取壓差設定值
            uint16_t pressure_sp_raw = read_holding_register(REG_PRESSURE_SETPOINT);
            debug(tag, "pressure_sp_raw=%d (HMI)(%d)", pressure_sp_raw, REG_PRESSURE_SETPOINT);
            if (pressure_sp_raw != 0xFFFF) {
                _controller.pressure_pid.setpoint = (float)pressure_sp_raw / 100.0f;
                _controller.pressure_pid.enabled = true;
            }
            execute_pressure_control(&_controller, dt);
        }
    }
    
    // 更新統計
    _controller.cycle_count++;
    
    return 0;
}

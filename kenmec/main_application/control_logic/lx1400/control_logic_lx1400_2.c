/*
 * control_logic_lx1400_2.c - LX1400 壓力控制邏輯 (Control Logic 2: Pressure Control)
 *
 * 【功能概述】
 * 本模組實現 LX1400 機型的壓差控制功能,通過 PID 演算法維持冷卻水系統的壓差穩定。
 * 相較於 LS80 機型,LX1400 使用不同的感測器地址配置。
 *
 * 【控制目標】
 * - 維持二次側進出水壓差在設定值 (P_set)
 * - 壓差 = 進水壓力 - 出水壓力
 *
 * 【感測器配置】
 * - P1 (REG 42082): 一次側進水壓力 (參考)
 * - P9 (REG 42090): 一次側出水壓力 (參考)
 * - P11 (REG 42092): 二次側進水壓力1 (主要控制)
 * - P12 (REG 42093): 二次側進水壓力2 (主要控制)
 * - P17 (REG 42098): 二次側出水壓力1 (主要控制)
 * - P18 (REG 42099): 二次側出水壓力2 (主要控制)
 *
 * 【執行器控制】
 * - Pump1/2/3: 泵浦速度 0-100% (通過 0-10V 類比輸出控制)
 * - 比例閥: 開度 10-100%
 *
 * 【控制模式】
 * - 手動模式: 僅監控,不自動調整
 * - 自動模式: PID 控制 + 泵浦協調策略
 *
 * 【泵浦協調策略】
 * 根據 PID 輸出自動決定運行泵浦數量:
 * - PID > 70%: 三泵運行 (高負載)
 * - PID > 50%: 雙泵運行 (中等負載)
 * - PID > 30%: 單泵運行 (低負載)
 * - PID ≤ 30%: 待機狀態
 *
 * 【安全保護】
 * - 最大壓力限制: 8.0 bar
 * - 壓力警報閾值: 7.0 bar
 * - 壓力停機閾值: 8.5 bar
 * - 最大壓差限制: 3.0 bar
 * - 緊急停機程序: 停止所有泵浦 + 關閉比例閥
 *
 * 【PID 參數】
 * - Kp: 2.0 (比例增益)
 * - Ki: 0.5 (積分增益)
 * - Kd: 0.1 (微分增益)
 * - 積分飽和限制: ±50
 *
 * 作者: [DK] - LX1400 機型適配
 * 日期: 2025
 */

#include "dexatek/main_application/include/application_common.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
static const char *debug_tag = "lx1400_2_pressure";  // 日誌標籤


#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_lx1400_2.json"
#define CONFIG_REGISTER_LIST_SIZE 25
static control_logic_register_t _control_logic_register_list[CONFIG_REGISTER_LIST_SIZE];

// Modbus寄存器定義 (根據cdu_pressure_control_logic_2_1.md規格)

static uint32_t REG_CONTROL_LOGIC_2_ENABLE = 41002; // 控制邏輯2啟用

// 壓力監測寄存器
static uint32_t REG_P11_INLET_PRESSURE = 42092;   // P11二次側進水壓力1
static uint32_t REG_P12_INLET_PRESSURE = 42093;   // P12二次側進水壓力2

// #define REG_P17_OUTLET_PRESSURE    42097   // P17二次側出水壓力1  
// #define REG_P18_OUTLET_PRESSURE    42098   // P18二次側出水壓力2
static uint32_t REG_P17_OUTLET_PRESSURE = 42098;   // P17二次側出水壓力1 // [DK]
static uint32_t REG_P18_OUTLET_PRESSURE = 42099;   // P18二次側出水壓力2 // [DK]

static uint32_t REG_P1_PRIMARY_INLET = 42082;   // P1一次側進水壓力 (參考)
// #define REG_P9_PRIMARY_OUTLET      42089   // P9一次側出水壓力 (參考)
static uint32_t REG_P9_PRIMARY_OUTLET = 42090;   // P9一次側出水壓力 (參考) // [DK]

// 控制設定寄存器
static uint32_t REG_PRESSURE_SETPOINT = 45002;   // P_set壓差設定值
static uint32_t REG_CONTROL_MODE = 45005;   // 控制模式 (0=流量, 1=壓差)

// 泵浦控制寄存器
static uint32_t REG_PUMP1_SPEED = 411037;  // 泵1轉速設定 (0-1000)
static uint32_t REG_PUMP2_SPEED = 411039;  // 泵2轉速設定
static uint32_t REG_PUMP3_SPEED = 411041;  // 泵3轉速設定
static uint32_t REG_PUMP1_CONTROL = 411101;  // 泵1啟停控制
static uint32_t REG_PUMP2_CONTROL = 411103;  // 泵2啟停控制
static uint32_t REG_PUMP3_CONTROL = 411105;  // 泵3啟停控制

// 手動模式控制寄存器
static uint32_t REG_PUMP1_MANUAL = 45021;   // 泵1手動模式
static uint32_t REG_PUMP2_MANUAL = 45022;   // 泵2手動模式
static uint32_t REG_PUMP3_MANUAL = 45023;   // 泵3手動模式
static uint32_t REG_VALVE_MANUAL = 45061;   // 閥門手動模式

// 比例閥控制寄存器
static uint32_t REG_VALVE_SETPOINT = 411147;  // 閥門開度設定 (%)
static uint32_t REG_VALVE_ACTUAL = 411161;  // 閥門實際開度 (%)

// 安全保護寄存器
static uint32_t REG_HIGH_PRESSURE_ALARM = 46271;   // 高壓警報
static uint32_t REG_HIGH_PRESSURE_SHUTDOWN = 46272;  // 高壓停機

// 安全限制常數 (根據規格)
#define MAX_PRESSURE_LIMIT         8.0f    // 最大允許壓力 8.0 bar
#define MIN_PRESSURE_LIMIT         1.0f    // 最小允許壓力 1.0 bar
#define MAX_PRESSURE_DIFF_LIMIT    3.0f    // 最大允許壓差 3.0 bar
#define PRESSURE_ALARM_THRESHOLD   7.0f    // 壓力警報閾值 7.0 bar
#define PRESSURE_SHUTDOWN_THRESHOLD 8.5f   // 壓力停機閾值 8.5 bar
#define MIN_PUMP_SPEED             20.0f   // 泵浦最小速度 20%
#define MAX_PUMP_SPEED             100.0f  // 泵浦最大速度 100%

/*---------------------------------------------------------------------------
								Variables
 ---------------------------------------------------------------------------*/
typedef enum {
    PRESSURE_CONTROL_MODE_FLOW = 0,      // 流量控制模式
    PRESSURE_CONTROL_MODE_PRESSURE = 1   // 壓差控制模式
} control_mode_t;

typedef enum {
    MANUAL_MODE_AUTO = 0,
    MANUAL_MODE_MANUAL = 1
} manual_mode_t;

typedef enum {
    SAFETY_LEVEL_SAFE = 0,
    SAFETY_LEVEL_WARNING = 1,
    SAFETY_LEVEL_CRITICAL = 2,
    SAFETY_LEVEL_EMERGENCY = 3
} safety_level_t;

typedef struct {
    float P11_inlet_pressure1;      // 二次側進水壓力1 (bar)
    float P12_inlet_pressure2;      // 二次側進水壓力2 (bar)
    float P17_outlet_pressure1;     // 二次側出水壓力1 (bar)
    float P18_outlet_pressure2;     // 二次側出水壓力2 (bar)
    float avg_inlet_pressure;       // 平均進水壓力
    float avg_outlet_pressure;      // 平均出水壓力
    float pressure_differential;    // 壓差 (進水 - 出水)
    time_t timestamp;
} pressure_sensor_data_t;

typedef struct {
    float kp;                       // 比例增益
    float ki;                       // 積分增益
    float kd;                       // 微分增益
    float integral;                 // 積分項累積
    float previous_error;           // 前一次誤差
    time_t previous_time;           // 前一次計算時間
    float output_min;               // 輸出最小值
    float output_max;               // 輸出最大值
} pressure_pid_controller_t;

typedef struct {
    int pump1_running;              // Pump1運行狀態
    int pump2_running;              // Pump2運行狀態
    int pump3_running;              // Pump3運行狀態
    float pump1_speed;              // Pump1速度 0-100%
    float pump2_speed;              // Pump2速度 0-100%
    float pump3_speed;              // Pump3速度 0-100%
    float valve_opening;            // 比例閥開度 0-100%
} pump_coordination_strategy_t;

typedef struct {
    safety_level_t level;
    int safe;
    char alarms[5][128];           // 最多5個警報訊息
    int alarm_count;
    int shutdown_required;
} pressure_safety_result_t;

// 全域變數
static pressure_pid_controller_t pressure_pid = {
    .kp = 2.0f,                    // 比例增益 (根據規格)
    .ki = 0.5f,                    // 積分增益
    .kd = 0.1f,                    // 微分增益
    .integral = 0.0f,
    .previous_error = 0.0f,
    .previous_time = 0,
    .output_min = 0.0f,
    .output_max = 100.0f
};

/*---------------------------------------------------------------------------
							Function Prototypes
 ---------------------------------------------------------------------------*/
// 函數宣告
static uint16_t modbus_read_input_register(uint32_t address);
static bool modbus_write_single_register(uint32_t address, uint16_t value);

static int read_pressure_sensor_data(pressure_sensor_data_t *data);
static float calculate_pressure_differential(const pressure_sensor_data_t *data);
static int check_control_mode(void);
static int check_manual_mode(void);
static int execute_manual_pressure_control(float target_pressure_diff);
static int execute_automatic_pressure_control(const pressure_sensor_data_t *data);
static float calculate_pressure_pid_output(pressure_pid_controller_t *pid, float setpoint, float current_value);
static void reset_pressure_pid_controller(pressure_pid_controller_t *pid);
static int calculate_pump_coordination_strategy(float pid_output, pump_coordination_strategy_t *strategy);
static int execute_pump_coordination_control(const pump_coordination_strategy_t *strategy);
static int adjust_proportional_valve(float pid_output, float current_valve_position);
static int perform_pressure_safety_checks(const pressure_sensor_data_t *data, pressure_safety_result_t *result);
static int handle_safety_issue(const pressure_safety_result_t *safety_result);
static void emergency_pressure_shutdown(void);
// static int pump_overload_recovery(int pump_id);

/*---------------------------------------------------------------------------
                                Implementation
 ---------------------------------------------------------------------------*/
static uint16_t modbus_read_input_register(uint32_t address) {    
    uint16_t value = 0;
    int ret = control_logic_read_holding_register(address, &value);

    if (ret != SUCCESS) {
        value = 0xFFFF;
    }

    return value;
}

static bool modbus_write_single_register(uint32_t address, uint16_t value) {
   
    int ret = control_logic_write_register(address, value, 2000);

    return (ret == SUCCESS)? true : false;
}

static int _register_list_init(void)
{
    int ret = SUCCESS;

    _control_logic_register_list[0].name = REG_CONTROL_LOGIC_2_ENABLE_STR;
    _control_logic_register_list[0].address_ptr = &REG_CONTROL_LOGIC_2_ENABLE;
    _control_logic_register_list[0].default_address = REG_CONTROL_LOGIC_2_ENABLE;
    _control_logic_register_list[0].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[1].name = REG_P1_PRESSURE_STR;
    _control_logic_register_list[1].address_ptr = &REG_P1_PRIMARY_INLET;
    _control_logic_register_list[1].default_address = REG_P1_PRIMARY_INLET;
    _control_logic_register_list[1].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[2].name = REG_P9_PRESSURE_STR;
    _control_logic_register_list[2].address_ptr = &REG_P9_PRIMARY_OUTLET;
    _control_logic_register_list[2].default_address = REG_P9_PRIMARY_OUTLET;
    _control_logic_register_list[2].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[3].name = REG_P11_PRESSURE_STR;
    _control_logic_register_list[3].address_ptr = &REG_P11_INLET_PRESSURE;
    _control_logic_register_list[3].default_address = REG_P11_INLET_PRESSURE;
    _control_logic_register_list[3].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[4].name = REG_P12_PRESSURE_STR;
    _control_logic_register_list[4].address_ptr = &REG_P12_INLET_PRESSURE;
    _control_logic_register_list[4].default_address = REG_P12_INLET_PRESSURE;
    _control_logic_register_list[4].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[5].name = REG_P17_PRESSURE_STR;
    _control_logic_register_list[5].address_ptr = &REG_P17_OUTLET_PRESSURE;
    _control_logic_register_list[5].default_address = REG_P17_OUTLET_PRESSURE;
    _control_logic_register_list[5].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[6].name = REG_P18_PRESSURE_STR;
    _control_logic_register_list[6].address_ptr = &REG_P18_OUTLET_PRESSURE;
    _control_logic_register_list[6].default_address = REG_P18_OUTLET_PRESSURE;
    _control_logic_register_list[6].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[7].name = REG_PRESSURE_SETPOINT_STR;
    _control_logic_register_list[7].address_ptr = &REG_PRESSURE_SETPOINT;
    _control_logic_register_list[7].default_address = REG_PRESSURE_SETPOINT;
    _control_logic_register_list[7].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[8].name = REG_FLOW_MODE_STR;
    _control_logic_register_list[8].address_ptr = &REG_CONTROL_MODE;
    _control_logic_register_list[8].default_address = REG_CONTROL_MODE;
    _control_logic_register_list[8].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[9].name = REG_PUMP1_SPEED_STR;
    _control_logic_register_list[9].address_ptr = &REG_PUMP1_SPEED;
    _control_logic_register_list[9].default_address = REG_PUMP1_SPEED;
    _control_logic_register_list[9].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[10].name = REG_PUMP2_SPEED_STR;
    _control_logic_register_list[10].address_ptr = &REG_PUMP2_SPEED;
    _control_logic_register_list[10].default_address = REG_PUMP2_SPEED;
    _control_logic_register_list[10].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[11].name = REG_PUMP3_SPEED_STR;
    _control_logic_register_list[11].address_ptr = &REG_PUMP3_SPEED;
    _control_logic_register_list[11].default_address = REG_PUMP3_SPEED;
    _control_logic_register_list[11].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[12].name = REG_PUMP1_CONTROL_STR;
    _control_logic_register_list[12].address_ptr = &REG_PUMP1_CONTROL;
    _control_logic_register_list[12].default_address = REG_PUMP1_CONTROL;
    _control_logic_register_list[12].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[13].name = REG_PUMP2_CONTROL_STR;
    _control_logic_register_list[13].address_ptr = &REG_PUMP2_CONTROL;
    _control_logic_register_list[13].default_address = REG_PUMP2_CONTROL;
    _control_logic_register_list[13].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[14].name = REG_PUMP3_CONTROL_STR;
    _control_logic_register_list[14].address_ptr = &REG_PUMP3_CONTROL;
    _control_logic_register_list[14].default_address = REG_PUMP3_CONTROL;
    _control_logic_register_list[14].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[15].name = REG_PUMP1_MANUAL_MODE_STR;
    _control_logic_register_list[15].address_ptr = &REG_PUMP1_MANUAL;
    _control_logic_register_list[15].default_address = REG_PUMP1_MANUAL;
    _control_logic_register_list[15].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[16].name = REG_PUMP2_MANUAL_MODE_STR;
    _control_logic_register_list[16].address_ptr = &REG_PUMP2_MANUAL;
    _control_logic_register_list[16].default_address = REG_PUMP2_MANUAL;
    _control_logic_register_list[16].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[17].name = REG_PUMP3_MANUAL_MODE_STR;
    _control_logic_register_list[17].address_ptr = &REG_PUMP3_MANUAL;
    _control_logic_register_list[17].default_address = REG_PUMP3_MANUAL;
    _control_logic_register_list[17].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[18].name = REG_VALVE_MANUAL_MODE_STR;
    _control_logic_register_list[18].address_ptr = &REG_VALVE_MANUAL;
    _control_logic_register_list[18].default_address = REG_VALVE_MANUAL;
    _control_logic_register_list[18].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[19].name = REG_VALVE_SETPOINT_STR;
    _control_logic_register_list[19].address_ptr = &REG_VALVE_SETPOINT;
    _control_logic_register_list[19].default_address = REG_VALVE_SETPOINT;
    _control_logic_register_list[19].type = CONTROL_LOGIC_REGISTER_WRITE;


    _control_logic_register_list[20].name = REG_VALVE_ACTUAL_STR;
    _control_logic_register_list[20].address_ptr = &REG_VALVE_ACTUAL;
    _control_logic_register_list[20].default_address = REG_VALVE_ACTUAL;
    _control_logic_register_list[20].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[21].name = REG_HIGH_PRESSURE_ALARM_STR;
    _control_logic_register_list[21].address_ptr = &REG_HIGH_PRESSURE_ALARM;
    _control_logic_register_list[21].default_address = REG_HIGH_PRESSURE_ALARM;
    _control_logic_register_list[21].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[22].name = REG_HIGH_PRESSURE_SHUTDOWN_STR;
    _control_logic_register_list[22].address_ptr = &REG_HIGH_PRESSURE_SHUTDOWN;
    _control_logic_register_list[22].default_address = REG_HIGH_PRESSURE_SHUTDOWN;
    _control_logic_register_list[22].type = CONTROL_LOGIC_REGISTER_READ_WRITE;
    
    uint32_t list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    ret = control_logic_register_load_from_file(CONFIG_REGISTER_FILE_PATH, _control_logic_register_list, list_size);
    debug(debug_tag, "load register array from file %s, ret %d", CONFIG_REGISTER_FILE_PATH, ret);

    return ret;
}

int control_logic_lx1400_2_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

int control_logic_lx1400_2_pressure_control_init(void) 
{

    // register list init
    _register_list_init();

    return SUCCESS;
}

/**
 * CDU壓差控制主要函數
 * 實現分階段部署：手動模式 + 自動壓差控制
 */
int control_logic_lx1400_2_pressure_control(ControlLogic *ptr) {
    pressure_sensor_data_t sensor_data;
    pressure_safety_result_t safety_result;
    int control_mode, ret = 0;
    
    (void)ptr;

    // check enable
    if (modbus_read_input_register(REG_CONTROL_LOGIC_2_ENABLE) != 1) {
        return 0;
    }

    info(debug_tag, "=== CDU壓差控制系統執行 ===");
    
    // 1. 檢查控制模式
    control_mode = check_control_mode();
    if (control_mode != PRESSURE_CONTROL_MODE_PRESSURE) {
        debug(debug_tag, "系統非壓差控制模式，跳過執行");
        return 0; // 非壓差模式，正常退出
    }
    
    // 2. 讀取壓力感測器數據
    if (read_pressure_sensor_data(&sensor_data) != 0) {
        error(debug_tag, "讀取壓力感測器數據失敗");
        return -1;
    }
    
    debug(debug_tag, "壓力數據 - 進水平均: %.2f bar, 出水平均: %.2f bar, 壓差: %.2f bar",
          sensor_data.avg_inlet_pressure, sensor_data.avg_outlet_pressure, sensor_data.pressure_differential);
    
    // 3. 執行安全檢查
    if (perform_pressure_safety_checks(&sensor_data, &safety_result) != 0) {
        error(debug_tag, "壓力安全檢查執行失敗");
        return -2;
    }
    
    // 4. 處理安全問題
    if (safety_result.level >= SAFETY_LEVEL_CRITICAL) {
        error(debug_tag, "檢測到嚴重安全問題，等級: %d", safety_result.level);
        handle_safety_issue(&safety_result);
        
        if (safety_result.shutdown_required) {
            error(debug_tag, "執行緊急停機程序");
            emergency_pressure_shutdown();
            return -3;
        }
    } else if (safety_result.level == SAFETY_LEVEL_WARNING) {
        warn(debug_tag, "壓力控制警告狀態，繼續監控");
    }
    
    // 5. 根據手動/自動模式執行相應控制
    int manual_mode = check_manual_mode();
    
    if (manual_mode) {
        info(debug_tag, "手動壓差控制模式");
        ret = execute_manual_pressure_control(sensor_data.pressure_differential);
    } else {
        info(debug_tag, "自動壓差控制模式");
        ret = execute_automatic_pressure_control(&sensor_data);
    }
    
    if (ret != 0) {
        error(debug_tag, "壓差控制邏輯執行失敗: %d", ret);
    }
    
    debug(debug_tag, "=== CDU壓差控制循環完成 ===");
    return ret;
}

/**
 * 讀取壓力感測器數據
 */
static int read_pressure_sensor_data(pressure_sensor_data_t *data) {
    if (data == NULL) {
        return -1;
    }
    
    int pressure_raw;
    
    // 讀取P11二次側進水壓力1
    pressure_raw = modbus_read_input_register(REG_P11_INLET_PRESSURE);
    if (pressure_raw >= 0) {
        data->P11_inlet_pressure1 = pressure_raw / 10.0f; // 0.1 bar精度
    } else {
        warn(debug_tag, "P11壓力讀取失敗");
        data->P11_inlet_pressure1 = 0.0f;
    }
    
    // 讀取P12二次側進水壓力2
    pressure_raw = modbus_read_input_register(REG_P12_INLET_PRESSURE);
    if (pressure_raw >= 0) {
        data->P12_inlet_pressure2 = pressure_raw / 10.0f;
    } else {
        warn(debug_tag, "P12壓力讀取失敗");
        data->P12_inlet_pressure2 = 0.0f;
    }
    
    // 讀取P17二次側出水壓力1
    pressure_raw = modbus_read_input_register(REG_P17_OUTLET_PRESSURE);
    if (pressure_raw >= 0) {
        data->P17_outlet_pressure1 = pressure_raw / 10.0f;
    } else {
        warn(debug_tag, "P17壓力讀取失敗");
        data->P17_outlet_pressure1 = 0.0f;
    }
    
    // 讀取P18二次側出水壓力2
    pressure_raw = modbus_read_input_register(REG_P18_OUTLET_PRESSURE);
    if (pressure_raw >= 0) {
        data->P18_outlet_pressure2 = pressure_raw / 10.0f;
    } else {
        warn(debug_tag, "P18壓力讀取失敗");
        data->P18_outlet_pressure2 = 0.0f;
    }
    
    // 計算平均壓力
    data->avg_inlet_pressure = (data->P11_inlet_pressure1 + data->P12_inlet_pressure2) / 2.0f;
    data->avg_outlet_pressure = (data->P17_outlet_pressure1 + data->P18_outlet_pressure2) / 2.0f;
    
    // 計算壓差
    data->pressure_differential = calculate_pressure_differential(data);
    
    // 設定時間戳
    data->timestamp = time(NULL);
    
    return 0;
}

/**
 * 計算壓差 (根據規格實現)
 */
static float calculate_pressure_differential(const pressure_sensor_data_t *data) {
    if (data == NULL) {
        return 0.0f;
    }
    
    // 壓差 = 進水壓力 - 出水壓力
    float pressure_diff = data->avg_inlet_pressure - data->avg_outlet_pressure;
    
    debug(debug_tag, "壓差計算: %.2f (進水) - %.2f (出水) = %.2f bar",
          data->avg_inlet_pressure, data->avg_outlet_pressure, pressure_diff);
    
    return pressure_diff;
}

/**
 * 檢查控制模式
 */
static int check_control_mode(void) {
    int mode = modbus_read_input_register(REG_CONTROL_MODE);
    if (mode < 0) {
        warn(debug_tag, "讀取控制模式失敗，預設為流量模式");
        return PRESSURE_CONTROL_MODE_FLOW;
    }
    
    return mode;
}

/**
 * 檢查手動模式
 */
static int check_manual_mode(void) {
    int pump1_manual = modbus_read_input_register(REG_PUMP1_MANUAL);
    int pump2_manual = modbus_read_input_register(REG_PUMP2_MANUAL);
    int pump3_manual = modbus_read_input_register(REG_PUMP3_MANUAL);
    int valve_manual = modbus_read_input_register(REG_VALVE_MANUAL);
    
    // 任何一個設備在手動模式，整個系統就是手動模式
    return (pump1_manual > 0 || pump2_manual > 0 || pump3_manual > 0 || valve_manual > 0);
}

/**
 * 手動壓差控制模式
 */
static int execute_manual_pressure_control(float target_pressure_diff) {
    info(debug_tag, "手動壓差控制 - 目標壓差: %.2f bar", target_pressure_diff);
    
    // 手動模式下僅監控狀態，不自動調整設備
    // 等待操作員通過HMI或其他介面手動調整泵浦和閥門
    
    debug(debug_tag, "手動模式：等待操作員手動調整泵浦和比例閥設定");
    
    return 0;
}

/**
 * 自動壓差控制模式
 */
static int execute_automatic_pressure_control(const pressure_sensor_data_t *data) {
    float target_pressure_diff, pid_output;
    pump_coordination_strategy_t pump_strategy;
    int target_raw;
    
    info(debug_tag, "自動壓差控制模式執行");
    
    // 讀取目標壓差設定
    target_raw = modbus_read_input_register(REG_PRESSURE_SETPOINT);
    if (target_raw >= 0) {
        target_pressure_diff = target_raw / 10.0f; // 0.1 bar精度
    } else {
        target_pressure_diff = 2.0f; // 預設目標壓差 2.0 bar
        warn(debug_tag, "讀取目標壓差失敗，使用預設值: %.1f bar", target_pressure_diff);
    }
    
    // PID壓差控制計算
    pid_output = calculate_pressure_pid_output(&pressure_pid, target_pressure_diff, data->pressure_differential);
    
    info(debug_tag, "壓差控制: 目標=%.2f bar, 實際=%.2f bar, 誤差=%.2f bar, PID輸出=%.1f%%",
         target_pressure_diff, data->pressure_differential,
         target_pressure_diff - data->pressure_differential, pid_output);
    
    // 計算泵浦協調策略
    if (calculate_pump_coordination_strategy(pid_output, &pump_strategy) != 0) {
        error(debug_tag, "泵浦協調策略計算失敗");
        return -1;
    }
    
    // 執行泵浦協調控制
    if (execute_pump_coordination_control(&pump_strategy) != 0) {
        error(debug_tag, "泵浦協調控制執行失敗");
        return -2;
    }
    
    // 調整比例閥聯動
    int current_valve_raw = modbus_read_input_register(REG_VALVE_ACTUAL);
    float current_valve_position = (current_valve_raw >= 0) ? current_valve_raw : 50.0f;
    
    if (adjust_proportional_valve(pid_output, current_valve_position) != 0) {
        warn(debug_tag, "比例閥調整失敗");
    }
    
    return 0;
}

/**
 * 壓差PID控制器計算
 */
static float calculate_pressure_pid_output(pressure_pid_controller_t *pid, float setpoint, float current_value) {
    time_t current_time = time(NULL);
    float delta_time = (current_time > pid->previous_time) ? (current_time - pid->previous_time) : 1.0f;
    
    // 計算控制誤差
    float error = setpoint - current_value;
    
    // 比例項
    float proportional = pid->kp * error;
    
    // 積分項 - 防積分飽和 (根據規格實現)
    pid->integral += error * delta_time;
    if (pid->integral > 50.0f) {
        pid->integral = 50.0f;
    } else if (pid->integral < -50.0f) {
        pid->integral = -50.0f;
    }
    float integral_term = pid->ki * pid->integral;
    
    // 微分項
    float derivative = (delta_time > 0) ? (error - pid->previous_error) / delta_time : 0.0f;
    float derivative_term = pid->kd * derivative;
    
    // PID輸出計算
    float output = proportional + integral_term + derivative_term;
    
    // 輸出限制
    if (output > pid->output_max) output = pid->output_max;
    if (output < pid->output_min) output = pid->output_min;
    
    // 更新狀態
    pid->previous_error = error;
    pid->previous_time = current_time;
    
    debug(debug_tag, "壓差PID - 誤差: %.3f, P: %.2f, I: %.2f, D: %.2f, 輸出: %.1f%%",
          error, proportional, integral_term, derivative_term, output);
    
    return output;
}

/**
 * 重置壓差PID控制器
 */
static void reset_pressure_pid_controller(pressure_pid_controller_t *pid) {
    pid->integral = 0.0f;
    pid->previous_error = 0.0f;
    pid->previous_time = time(NULL);
    debug(debug_tag, "壓差PID控制器已重置");
}

/**
 * 計算泵浦協調策略 (根據規格實現)
 */
static int calculate_pump_coordination_strategy(float pid_output, pump_coordination_strategy_t *strategy) {
    if (strategy == NULL) {
        return -1;
    }
    
    // 初始化策略
    memset(strategy, 0, sizeof(pump_coordination_strategy_t));
    
    if (pid_output > 0) {
        // 需要增加壓力/流量
        if (pid_output > 70.0f) {
            // 三泵運行 - 高負載
            strategy->pump1_running = 1;
            strategy->pump2_running = 1;
            strategy->pump3_running = 1;
            strategy->pump1_speed = MIN_PUMP_SPEED + (pid_output - 70.0f) * 0.8f;
            strategy->pump2_speed = MIN_PUMP_SPEED + (pid_output - 70.0f) * 0.8f;
            strategy->pump3_speed = MIN_PUMP_SPEED + (pid_output - 70.0f) * 0.8f;
            info(debug_tag, "三泵運行策略 - PID輸出: %.1f%%", pid_output);
            
        } else if (pid_output > 50.0f) {
            // 雙泵運行 - 中等負載
            strategy->pump1_running = 1;
            strategy->pump2_running = 1;
            strategy->pump3_running = 0;
            strategy->pump1_speed = MIN_PUMP_SPEED + (pid_output - 50.0f) * 1.2f;
            strategy->pump2_speed = MIN_PUMP_SPEED + (pid_output - 50.0f) * 1.2f;
            strategy->pump3_speed = 0.0f;
            info(debug_tag, "雙泵運行策略 - PID輸出: %.1f%%", pid_output);
            
        } else if (pid_output > 30.0f) {
            // 單泵運行 - 低負載
            strategy->pump1_running = 1;
            strategy->pump2_running = 0;
            strategy->pump3_running = 0;
            strategy->pump1_speed = MIN_PUMP_SPEED + (pid_output - 30.0f) * 2.0f;
            strategy->pump2_speed = 0.0f;
            strategy->pump3_speed = 0.0f;
            info(debug_tag, "單泵運行策略 - PID輸出: %.1f%%", pid_output);
            
        } else {
            // 待機狀態 - 極低負載
            strategy->pump1_running = 0;
            strategy->pump2_running = 0;
            strategy->pump3_running = 0;
            strategy->pump1_speed = 0.0f;
            strategy->pump2_speed = 0.0f;
            strategy->pump3_speed = 0.0f;
            debug(debug_tag, "待機狀態 - PID輸出過低: %.1f%%", pid_output);
        }
    } else {
        // PID輸出為負或零，減少或停止泵浦運行
        strategy->pump1_running = 0;
        strategy->pump2_running = 0;
        strategy->pump3_running = 0;
        strategy->pump1_speed = 0.0f;
        strategy->pump2_speed = 0.0f;
        strategy->pump3_speed = 0.0f;
        debug(debug_tag, "泵浦停止策略 - PID輸出: %.1f%%", pid_output);
    }
    
    // 限制速度範圍
    if (strategy->pump1_speed > MAX_PUMP_SPEED) strategy->pump1_speed = MAX_PUMP_SPEED;
    if (strategy->pump2_speed > MAX_PUMP_SPEED) strategy->pump2_speed = MAX_PUMP_SPEED;
    if (strategy->pump3_speed > MAX_PUMP_SPEED) strategy->pump3_speed = MAX_PUMP_SPEED;
    
    return 0;
}

/**
 * 執行泵浦協調控制
 */
static int execute_pump_coordination_control(const pump_coordination_strategy_t *strategy) {
    if (strategy == NULL) {
        return -1;
    }
    
    // 控制Pump1
    if (strategy->pump1_running) {
        int speed_value = (int)(strategy->pump1_speed * 10.0f); // 轉換為0-1000範圍
        // [DK]: 1000 means 10V (10000mV) => speed_cmd * 10 
        speed_value = speed_value * 10; // [DK]
        modbus_write_single_register(REG_PUMP1_SPEED, speed_value);
        modbus_write_single_register(REG_PUMP1_CONTROL, 1);
        debug(debug_tag, "Pump1啟動 - 速度: %.1f%% (%d)", strategy->pump1_speed, speed_value);
    } else {
        modbus_write_single_register(REG_PUMP1_CONTROL, 0);
        debug(debug_tag, "Pump1停止");
    }
    
    // 控制Pump2
    if (strategy->pump2_running) {
        int speed_value = (int)(strategy->pump2_speed * 10.0f);
        // [DK]: 1000 means 10V (10000mV) => speed_cmd * 10 
        speed_value = speed_value * 10; // [DK]
        modbus_write_single_register(REG_PUMP2_SPEED, speed_value);
        modbus_write_single_register(REG_PUMP2_CONTROL, 1);
        debug(debug_tag, "Pump2啟動 - 速度: %.1f%% (%d)", strategy->pump2_speed, speed_value);
    } else {
        modbus_write_single_register(REG_PUMP2_CONTROL, 0);
        debug(debug_tag, "Pump2停止");
    }
    
    // 控制Pump3
    if (strategy->pump3_running) {
        int speed_value = (int)(strategy->pump3_speed * 10.0f);
        // [DK]: 1000 means 10V (10000mV) => speed_cmd * 10 
        speed_value = speed_value * 10; // [DK]
        modbus_write_single_register(REG_PUMP3_SPEED, speed_value);
        modbus_write_single_register(REG_PUMP3_CONTROL, 1);
        debug(debug_tag, "Pump3啟動 - 速度: %.1f%% (%d)", strategy->pump3_speed, speed_value);
    } else {
        modbus_write_single_register(REG_PUMP3_CONTROL, 0);
        debug(debug_tag, "Pump3停止");
    }
    
    return 0;
}

/**
 * 調整比例閥聯動控制
 */
static int adjust_proportional_valve(float pid_output, float current_valve_position) {
    float valve_adjustment = pid_output * 0.1f; // 閥門響應係數
    float new_valve_position = current_valve_position + valve_adjustment;
    
    // 閥門開度限制 (10-100%)
    if (new_valve_position > 100.0f) new_valve_position = 100.0f;
    if (new_valve_position < 10.0f) new_valve_position = 10.0f;
    
    // 寫入新的閥門開度設定
    int valve_value = (int)(new_valve_position);

    modbus_write_single_register(REG_VALVE_SETPOINT, valve_value);
    
    debug(debug_tag, "比例閥調整: %.1f%% -> %.1f%% (調整量: %.2f%%)",
          current_valve_position, new_valve_position, valve_adjustment);
    
    return 0;
}

/**
 * 執行壓力安全檢查
 */
static int perform_pressure_safety_checks(const pressure_sensor_data_t *data, pressure_safety_result_t *result) {
    if (data == NULL || result == NULL) {
        return -1;
    }
    
    // 初始化安全結果
    result->level = SAFETY_LEVEL_SAFE;
    result->safe = 1;
    result->alarm_count = 0;
    result->shutdown_required = 0;
    
    // 1. 出水壓力過高檢查 (最嚴重)
    if (data->avg_outlet_pressure > PRESSURE_SHUTDOWN_THRESHOLD) {
        result->level = SAFETY_LEVEL_EMERGENCY;
        result->safe = 0;
        result->shutdown_required = 1;
        snprintf(result->alarms[result->alarm_count], sizeof(result->alarms[0]),
                "緊急: 出水壓力過高 %.2f bar > %.2f bar", 
                data->avg_outlet_pressure, PRESSURE_SHUTDOWN_THRESHOLD);
        result->alarm_count++;
        
        // 寫入停機寄存器
        modbus_write_single_register(REG_HIGH_PRESSURE_SHUTDOWN, 1);
        
    } else if (data->avg_outlet_pressure > PRESSURE_ALARM_THRESHOLD) {
        result->level = SAFETY_LEVEL_WARNING;
        result->safe = 0;
        snprintf(result->alarms[result->alarm_count], sizeof(result->alarms[0]),
                "警告: 出水壓力過高 %.2f bar > %.2f bar", 
                data->avg_outlet_pressure, PRESSURE_ALARM_THRESHOLD);
        result->alarm_count++;
        
        // 寫入警報寄存器
        modbus_write_single_register(REG_HIGH_PRESSURE_ALARM, 1);
    }
    
    // 2. 進水壓力過低檢查
    if (data->avg_inlet_pressure < MIN_PRESSURE_LIMIT) {
        result->level = (result->level < SAFETY_LEVEL_CRITICAL) ? SAFETY_LEVEL_CRITICAL : result->level;
        result->safe = 0;
        snprintf(result->alarms[result->alarm_count], sizeof(result->alarms[0]),
                "嚴重: 進水壓力過低 %.2f bar < %.2f bar", 
                data->avg_inlet_pressure, MIN_PRESSURE_LIMIT);
        result->alarm_count++;
    }
    
    // 3. 壓差超限檢查
    if (fabs(data->pressure_differential) > MAX_PRESSURE_DIFF_LIMIT) {
        result->level = (result->level < SAFETY_LEVEL_WARNING) ? SAFETY_LEVEL_WARNING : result->level;
        result->safe = 0;
        snprintf(result->alarms[result->alarm_count], sizeof(result->alarms[0]),
                "警告: 壓差超限 %.2f bar > %.2f bar", 
                fabs(data->pressure_differential), MAX_PRESSURE_DIFF_LIMIT);
        result->alarm_count++;
    }
    
    // 4. 壓力感測器一致性檢查
    float inlet_pressure_diff = fabs(data->P11_inlet_pressure1 - data->P12_inlet_pressure2);
    float outlet_pressure_diff = fabs(data->P17_outlet_pressure1 - data->P18_outlet_pressure2);
    
    if (inlet_pressure_diff > 0.5f) {
        result->level = (result->level < SAFETY_LEVEL_WARNING) ? SAFETY_LEVEL_WARNING : result->level;
        result->safe = 0;
        snprintf(result->alarms[result->alarm_count], sizeof(result->alarms[0]),
                "警告: 進水壓力感測器差異過大 %.2f bar", inlet_pressure_diff);
        result->alarm_count++;
    }
    
    if (outlet_pressure_diff > 0.5f) {
        result->level = (result->level < SAFETY_LEVEL_WARNING) ? SAFETY_LEVEL_WARNING : result->level;
        result->safe = 0;
        snprintf(result->alarms[result->alarm_count], sizeof(result->alarms[0]),
                "警告: 出水壓力感測器差異過大 %.2f bar", outlet_pressure_diff);
        result->alarm_count++;
    }
    
    return 0;
}

/**
 * 處理安全問題
 */
static int handle_safety_issue(const pressure_safety_result_t *safety_result) {
    if (safety_result == NULL) {
        return -1;
    }
    
    // 記錄所有警報訊息
    for (int i = 0; i < safety_result->alarm_count; i++) {
        if (safety_result->level >= SAFETY_LEVEL_CRITICAL) {
            error(debug_tag, "安全警報: %s", safety_result->alarms[i]);
        } else {
            warn(debug_tag, "安全警報: %s", safety_result->alarms[i]);
        }
    }
    
    // 根據安全等級採取相應措施
    switch (safety_result->level) {
        case SAFETY_LEVEL_EMERGENCY:
            error(debug_tag, "緊急狀況，執行緊急停機");
            emergency_pressure_shutdown();
            break;
            
        case SAFETY_LEVEL_CRITICAL:
            warn(debug_tag, "嚴重狀況，降低系統負載");
            // 降低所有運行泵浦速度到最小
            // [DK]: 1000 means 10V (10000mV) => speed_cmd * 10 
            int min_pump_speed = MIN_PUMP_SPEED * 10; // [DK]
            modbus_write_single_register(REG_PUMP1_SPEED, min_pump_speed); // [DK]
            modbus_write_single_register(REG_PUMP2_SPEED, min_pump_speed); // [DK]
            modbus_write_single_register(REG_PUMP3_SPEED, min_pump_speed); // [DK]
            break;
            
        case SAFETY_LEVEL_WARNING:
            debug(debug_tag, "警告狀況，繼續監控");
            break;
            
        default:
            break;
    }
    
    return 0;
}

/**
 * 緊急壓力停機程序
 */
static void emergency_pressure_shutdown(void) {
    error(debug_tag, "執行緊急壓力停機程序...");
    
    // 停止所有泵浦
    modbus_write_single_register(REG_PUMP1_CONTROL, 0);
    modbus_write_single_register(REG_PUMP2_CONTROL, 0);
    modbus_write_single_register(REG_PUMP3_CONTROL, 0);
    
    // 關閉比例閥到最小開度
    uint16_t valve_value = 10;
    modbus_write_single_register(REG_VALVE_SETPOINT, valve_value); // 10%最小開度
    
    // 重置PID控制器
    reset_pressure_pid_controller(&pressure_pid);
    
    error(debug_tag, "緊急壓力停機完成");
}
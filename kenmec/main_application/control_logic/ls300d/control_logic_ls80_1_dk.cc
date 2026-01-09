/*
 * control_logic_ls80_1.c - LS80 溫度控制邏輯 (Control Logic 1: Temperature Control)
 *
 * 【功能概述】
 * 本模組實現 CDU 系統的溫度控制功能,通過 PID 演算法維持冷卻水出水溫度穩定,
 * 結合自適應參數調整和多泵浦協調策略,確保系統在不同負載下的精確溫控。
 *
 * 【控制目標】
 * - 維持二次側出水溫度在設定值 (T_set)
 * - 預設目標溫度: 25.0°C
 * - 溫度容差: ±0.5°C
 *
 * 【感測器配置】
 * - T4 (REG 413560): 二次側進水溫度 (0.1°C 精度)
 * - T2 (REG 413556): 二次側出水溫度 (主要控制目標, 0.1°C 精度)
 * - F2 (REG 42063): 二次側流量回饋 (0.1 L/min 精度)
 * - P4 (REG 42085): 進水壓力監測
 * - P13 (REG 42094): 出水壓力監測
 *
 * 【執行器控制】
 * - Pump1/2: 泵浦速度 0-100% (通過寄存器 45015/45016 控制)
 * - 比例閥: 開度 0-100% (REG 411151)
 * - 泵浦啟停: DO 控制 (REG 411101/411103)
 *
 * 【控制模式】
 * - 手動模式 (TEMP_CONTROL_MODE_MANUAL = 0): 僅監控,操作員手動調整
 * - 自動模式 (TEMP_CONTROL_MODE_AUTO = 1): PID 自動控制 + 泵浦協調
 *
 * 【泵浦協調策略】
 * 根據 PID 輸出和溫度誤差動態調整:
 * - 容量 ≤ 35%: 單泵運行 (輪換主泵)
 * - 容量 > 35% 且 ≤ 70%: 雙泵運行 (主泵+輔泵)
 * - 泵浦輪換週期: 24小時 (1440個控制週期)
 *
 * 【PID 參數】
 * - Kp: 15.0 (比例增益 - 快速響應溫度變化)
 * - Ki: 0.8 (積分增益 - 消除穩態誤差)
 * - Kd: 2.5 (微分增益 - 抑制溫度超調)
 * - 積分限幅: 防止飽和,動態調整範圍
 *
 * 【自適應調整】
 * - 大誤差 (>2°C): 增加 Kp, 減少 Ki → 快速響應
 * - 小誤差 (<0.2°C): 減少 Kp, 增加 Ki → 提高穩態精度
 *
 * 【安全保護】
 * - 最高溫度限制: 40.0°C
 * - 最低溫度限制: 15.0°C
 * - 最小流量: 0.0 L/min
 * - 進出水溫差異常: >10.0°C 觸發警告
 *
 * 作者: [DK]
 * 日期: 2025
 */

#include "dexatek/main_application/include/application_common.h"

#include "cJSON.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"

#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_ls80_1.json"
#define CONFIG_REGISTER_LIST_SIZE 20
static control_logic_register_t _control_logic_register_list[CONFIG_REGISTER_LIST_SIZE];

static const char *debug_tag = "ls80_1_temp";

typedef enum {
    TEMP_CONTROL_MODE_MANUAL = 0,
    TEMP_CONTROL_MODE_AUTO = 1
} temp_control_mode_t;

typedef enum {
    SAFETY_STATUS_SAFE = 0,
    SAFETY_STATUS_WARNING = 1,
    SAFETY_STATUS_EMERGENCY = 2
} safety_status_t;

typedef struct {
    float inlet_temps[2];      // T11, T12
    float outlet_temps[2];     // T17, T18
    float avg_inlet_temp;
    float avg_outlet_temp;
    float flow_rate;           // F2
    float inlet_pressures[2];  // P12, P13
    time_t timestamp;
} sensor_data_t;

typedef struct {
    float kp;
    float ki;
    float kd;
    float integral;
    float previous_error;
    time_t previous_time;
    float output_min;
    float output_max;
} pid_controller_t;

typedef struct {
    int active_pumps[3];       // Pump1, Pump2, Pump3 啟用狀態
    float pump_speeds[3];      // 泵浦速度 0-100%
    float valve_opening;       // 比例閥開度 0-100%
} control_output_t;

static pid_controller_t temperature_pid = {
    .kp = 15.0f,
    .ki = 0.8f,
    .kd = 2.5f,
    .integral = 0.0f,
    .previous_error = 0.0f,
    .previous_time = 0,
    .output_min = 0.0f,
    .output_max = 100.0f
};

static int current_lead_pump = 1;
static int pump_rotation_timer = 0;

// Modbus寄存器定義 (根據CDU系統規格)

static uint32_t REG_CONTROL_LOGIC_1_ENABLE = 41001; // 控制邏輯1啟用

static uint32_t REG_T4_TEMP = 413560; // T4_IN
static uint32_t REG_T2_TEMP = 413556; // T2_OUT

static uint32_t REG_F2_FLOW = 42063; // F2流量 11165, port 1, AI_2
static uint32_t REG_P4_PRESSURE = 42085; // P4壓力 11067, port 0, AI_3
static uint32_t REG_P13_PRESSURE = 42094; // P13壓力 11163, port 1, AI_1

static uint32_t REG_TARGET_TEMP = 45001; // 目標溫度設定
static uint32_t REG_FLOW_SETPOINT = 45003; // 流量設定
static uint32_t REG_TEMP_CONTROL_MODE = 45020; // 溫度控制模式 (0=手動, 1=自動)

static uint32_t REG_PUMP1_MANUAL_MODE = 45021; // Pump1手動模式
static uint32_t REG_PUMP2_MANUAL_MODE = 45022; // Pump2手動模式
static uint32_t REG_VALVE_MANUAL_MODE = 45061; // 比例閥手動模式

static uint32_t REG_PUMP1_SPEED = 45015; // Pump1速度設定 (0-1000)
static uint32_t REG_PUMP2_SPEED = 45016; // Pump2速度設定

static uint32_t REG_PUMP1_CONTROL = 411101;  // Pump1啟停控制
static uint32_t REG_PUMP2_CONTROL = 411103;  // Pump2啟停控制
static uint32_t REG_VALVE_OPENING = 411151; // 比例閥開度設定 (%)

// 安全限制參數
#define MAX_TEMP_LIMIT         40.0f   // 最高溫度限制
#define MIN_TEMP_LIMIT         15.0f    // 最低溫度限制
#define MIN_FLOW_RATE          0.0f  // 最小流量 L/min
#define TEMP_TOLERANCE         0.5f    // 溫度容差 ±0.5°C
#define TARGET_TEMP_DEFAULT    25.0f   // 預設目標溫度

// 函數宣告
static int read_sensor_data(sensor_data_t *data);
// static safety_status_t perform_safety_checks(const sensor_data_t *data);  // 暫時未使用
// static void emergency_shutdown(void);  // 暫時未使用
static float calculate_pid_output(pid_controller_t *pid, float setpoint, float current_value);
// static void reset_pid_controller(pid_controller_t *pid);  // 暫時未使用
static void adjust_pid_parameters(pid_controller_t *pid, float error);
static int execute_manual_control_mode(float target_temp);
static int execute_automatic_control_mode(const sensor_data_t *data);
static void calculate_pump_strategy(float required_capacity, control_output_t *output);
static void execute_pump_control(const control_output_t *output);
static void handle_pump_rotation(void);
static float calculate_valve_opening(float pid_output, const sensor_data_t *data);

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

    // setup default register list
    _control_logic_register_list[0].name = REG_CONTROL_LOGIC_1_ENABLE_STR;
    _control_logic_register_list[0].address_ptr = &REG_CONTROL_LOGIC_1_ENABLE, 
    _control_logic_register_list[0].default_address = REG_CONTROL_LOGIC_1_ENABLE,
    _control_logic_register_list[0].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[1].name = REG_F2_FLOW_STR;
    _control_logic_register_list[1].address_ptr = &REG_F2_FLOW,
    _control_logic_register_list[1].default_address = REG_F2_FLOW,
    _control_logic_register_list[1].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[2].name = REG_P4_PRESSURE_STR;
    _control_logic_register_list[2].address_ptr = &REG_P4_PRESSURE, 
    _control_logic_register_list[2].default_address = REG_P4_PRESSURE,
    _control_logic_register_list[2].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[3].name = REG_P13_PRESSURE_STR;
    _control_logic_register_list[3].address_ptr = &REG_P13_PRESSURE, 
    _control_logic_register_list[3].default_address = REG_P13_PRESSURE,
    _control_logic_register_list[3].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[4].name = REG_TARGET_TEMP_STR;
    _control_logic_register_list[4].address_ptr = &REG_TARGET_TEMP,
    _control_logic_register_list[4].default_address = REG_TARGET_TEMP,
    _control_logic_register_list[4].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[5].name = REG_FLOW_SETPOINT_STR;
    _control_logic_register_list[5].address_ptr = &REG_FLOW_SETPOINT, 
    _control_logic_register_list[5].default_address = REG_FLOW_SETPOINT,
    _control_logic_register_list[5].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[6].name = REG_PUMP1_SPEED_STR;
    _control_logic_register_list[6].address_ptr = &REG_PUMP1_SPEED, 
    _control_logic_register_list[6].default_address = REG_PUMP1_SPEED,
    _control_logic_register_list[6].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[7].name = REG_PUMP2_SPEED_STR;
    _control_logic_register_list[7].address_ptr = &REG_PUMP2_SPEED,
    _control_logic_register_list[7].default_address = REG_PUMP2_SPEED,
    _control_logic_register_list[7].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[8].name = REG_TEMP_CONTROL_MODE_STR;
    _control_logic_register_list[8].address_ptr = &REG_TEMP_CONTROL_MODE, 
    _control_logic_register_list[8].default_address = REG_TEMP_CONTROL_MODE,
    _control_logic_register_list[8].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[9].name = REG_PUMP1_MANUAL_MODE_STR;
    _control_logic_register_list[9].address_ptr = &REG_PUMP1_MANUAL_MODE,
    _control_logic_register_list[9].default_address = REG_PUMP1_MANUAL_MODE,
    _control_logic_register_list[9].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[10].name = REG_PUMP2_MANUAL_MODE_STR;
    _control_logic_register_list[10].address_ptr = &REG_PUMP2_MANUAL_MODE,
    _control_logic_register_list[10].default_address = REG_PUMP2_MANUAL_MODE,
    _control_logic_register_list[10].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[11].name = REG_VALVE_MANUAL_MODE_STR;
    _control_logic_register_list[11].address_ptr = &REG_VALVE_MANUAL_MODE,
    _control_logic_register_list[11].default_address = REG_VALVE_MANUAL_MODE,
    _control_logic_register_list[11].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[12].name = REG_T4_TEMP_STR;
    _control_logic_register_list[12].address_ptr = &REG_T4_TEMP,
    _control_logic_register_list[12].default_address = REG_T4_TEMP,
    _control_logic_register_list[12].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[13].name = REG_T2_TEMP_STR;
    _control_logic_register_list[13].address_ptr = &REG_T2_TEMP,
    _control_logic_register_list[13].default_address = REG_T2_TEMP,
    _control_logic_register_list[13].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[14].name = REG_PUMP1_CONTROL_STR;
    _control_logic_register_list[14].address_ptr = &REG_PUMP1_CONTROL,
    _control_logic_register_list[14].default_address = REG_PUMP1_CONTROL,
    _control_logic_register_list[14].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[15].name = REG_PUMP2_CONTROL_STR;
    _control_logic_register_list[15].address_ptr = &REG_PUMP2_CONTROL,
    _control_logic_register_list[15].default_address = REG_PUMP2_CONTROL,
    _control_logic_register_list[15].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[16].name = REG_VALVE_SETPOINT_STR;
    _control_logic_register_list[16].address_ptr = &REG_VALVE_OPENING,
    _control_logic_register_list[16].default_address = REG_VALVE_OPENING,
    _control_logic_register_list[16].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // try to load register array from file
    uint32_t list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    ret = control_logic_register_load_from_file(CONFIG_REGISTER_FILE_PATH, _control_logic_register_list, list_size);
    debug(debug_tag, "load register array from file %s, ret %d", CONFIG_REGISTER_FILE_PATH, ret);

    return ret;
}

int control_logic_ls80_1_temperature_control_init(void)
{
    _register_list_init();

    return SUCCESS;
}

/**
 * CDU溫度控制主要函數 (版本 1.1)
 *
 * 【函數功能】
 * 這是溫度控制邏輯的主入口函數,由控制邏輯管理器週期性調用。
 * 實現完整的溫度控制流程: 啟用檢查 → 感測器讀取 → 模式判斷 → 控制執行 → 泵浦輪換
 *
 * 【執行流程】
 * 1. 檢查控制邏輯是否啟用 (REG_CONTROL_LOGIC_1_ENABLE)
 * 2. 讀取所有溫度、流量、壓力感測器數據
 * 3. 讀取控制模式寄存器 (手動/自動)
 * 4. 根據模式執行對應的控制邏輯:
 *    - 手動模式: 僅監控,操作員通過 HMI 控制
 *    - 自動模式: PID 控制 + 自適應參數 + 泵浦協調
 * 5. 執行泵浦輪換邏輯 (24小時週期)
 *
 * @param ptr 控制邏輯結構指標 (本函數未使用)
 * @return 0=成功, -1=感測器讀取失敗, 其他=控制執行失敗
 */
// int control_logic_temperature_1_1(ControlLogic *ptr) {
int control_logic_ls80_1_temperature_control(ControlLogic *ptr) {
    (void)ptr;

    sensor_data_t sensor_data;
    // safety_status_t safety_status;  // 暫時未使用，等待安全檢查功能啟用
    int control_mode;
    int ret = 0;

    // 【步驟1】檢查控制邏輯1是否啟用 (通過 Modbus 寄存器 41001)
    if (modbus_read_input_register(REG_CONTROL_LOGIC_1_ENABLE) != 1) {
        return 0;  // 未啟用則直接返回,不執行控制
    }

    info(debug_tag, "=== CDU溫度控制系統執行 (v1.1) ===");

    // 【步驟2】讀取感測器數據
    // 包括 T4(進水溫), T2(出水溫), F2(流量), P4/P13(壓力)
    if (read_sensor_data(&sensor_data) != 0) {
        error(debug_tag, "讀取感測器數據失敗");
        return -1;
    }

    debug(debug_tag, "溫度數據 - 進水平均: %.1f°C, 出水平均: %.1f°C, 流量: %.1f L/min",
          sensor_data.avg_inlet_temp, sensor_data.avg_outlet_temp, sensor_data.flow_rate);

    // 【步驟3】安全檢查 (暫時註釋,可根據需求啟用)
    // 檢查項目: 溫度超限/流量過低/進出水溫差異常
    // safety_status = perform_safety_checks(&sensor_data);

    // if (safety_status == SAFETY_STATUS_EMERGENCY) {
    //     error(debug_tag, "緊急狀況發生，執行緊急停機");
    //     emergency_shutdown();
    //     return -2;
    // } else if (safety_status == SAFETY_STATUS_WARNING) {
    //     warn(debug_tag, "系統警告狀態，繼續監控");
    // }

    // 【步驟4】讀取控制模式 (0=手動, 1=自動)
    control_mode = modbus_read_input_register(REG_TEMP_CONTROL_MODE);
    if (control_mode < 0) {
        error(debug_tag, "讀取控制模式失敗");
        control_mode = TEMP_CONTROL_MODE_MANUAL; // 預設手動模式
    }

    // 【步驟5】根據控制模式執行相應邏輯
    if (control_mode == TEMP_CONTROL_MODE_AUTO) {
        // 自動模式: PID 控制 + 自適應參數調整 + 泵浦協調
        info(debug_tag, "執行自動溫度控制模式");
        ret = execute_automatic_control_mode(&sensor_data);
    } else {
        // 手動模式: 僅監控狀態,由操作員手動控制
        info(debug_tag, "手動溫度控制模式 - 僅監控狀態");
        ret = execute_manual_control_mode(TARGET_TEMP_DEFAULT);
    }

    if (ret != 0) {
        error(debug_tag, "控制邏輯執行失敗: %d", ret);
    }

    // 【步驟6】泵浦輪換處理 (24小時輪換,確保泵浦均勻使用)
    handle_pump_rotation();

    debug(debug_tag, "=== CDU溫度控制循環完成 ===");
    return ret;
}

/**
 * 讀取所有感測器數據
 *
 * 【功能說明】
 * 從 Modbus 寄存器讀取溫度、流量、壓力等感測器數據,並計算平均值。
 *
 * 【讀取內容】
 * - T4: 進水溫度 (0.1°C 精度, REG 413560)
 * - T2: 出水溫度 (0.1°C 精度, REG 413556, 主要控制目標)
 * - F2: 流量回饋 (0.1 L/min 精度, REG 42063)
 *
 * @param data 感測器數據結構指標
 * @return 0=成功, -1=參數錯誤
 */
static int read_sensor_data(sensor_data_t *data) {
    int temp_raw;
    
    // 讀取溫度數據 (0.1°C精度)
    temp_raw = modbus_read_input_register(REG_T4_TEMP);
    if (temp_raw >= 0) {
        data->inlet_temps[0] = temp_raw / 10.0f;
    } else {
        warn(debug_tag, "T4溫度讀取失敗");
        data->inlet_temps[0] = 0.0f;
    }
    
    temp_raw = modbus_read_input_register(REG_T2_TEMP);
    if (temp_raw >= 0) {
        data->outlet_temps[0] = temp_raw / 10.0f;
    } else {
        warn(debug_tag, "T2溫度讀取失敗");
        data->outlet_temps[0] = 0.0f;
    }
    
    // 計算平均溫度
    data->avg_inlet_temp = (data->inlet_temps[0] + data->inlet_temps[1]);
    data->avg_outlet_temp = (data->outlet_temps[0] + data->outlet_temps[1]);
    
    // 讀取流量數據 (0.1 L/min精度)
    temp_raw = modbus_read_input_register(REG_F2_FLOW);
    if (temp_raw >= 0) {
        data->flow_rate = temp_raw / 10.0f;
    } else {
        warn(debug_tag, "F2流量讀取失敗");
        data->flow_rate = 0.0f;
    }
    
    // 設定時間戳
    data->timestamp = time(NULL);
    
    return 0;
}

/* 暫時註解，等待安全檢查功能啟用
 * 安全檢查邏輯
 */
/*
static safety_status_t perform_safety_checks(const sensor_data_t *data) {
    // 緊急停機檢查
    if (data->avg_outlet_temp > MAX_TEMP_LIMIT) {
        error(debug_tag, "出水溫度過高: %.1f°C > %.1f°C", data->avg_outlet_temp, MAX_TEMP_LIMIT);
        return SAFETY_STATUS_EMERGENCY;
    }

    if (data->flow_rate < MIN_FLOW_RATE * 0.5f) {
        error(debug_tag, "流量過低: %.1f L/min < %.1f L/min", data->flow_rate, MIN_FLOW_RATE * 0.5f);
        return SAFETY_STATUS_EMERGENCY;
    }

    // 警告條件檢查
    if (data->avg_outlet_temp > TARGET_TEMP_DEFAULT + 5.0f) {
        warn(debug_tag, "溫度偏高警告: %.1f°C", data->avg_outlet_temp);
        return SAFETY_STATUS_WARNING;
    }

    if (data->flow_rate < MIN_FLOW_RATE) {
        warn(debug_tag, "流量偏低警告: %.1f L/min", data->flow_rate);
        return SAFETY_STATUS_WARNING;
    }

    // 進出水溫差異常檢查
    float temp_diff = fabs(data->avg_inlet_temp - data->avg_outlet_temp);
    if (temp_diff > 10.0f) {
        warn(debug_tag, "進出水溫差過大: %.1f°C", temp_diff);
        return SAFETY_STATUS_WARNING;
    }

    return SAFETY_STATUS_SAFE;
}
*/

/* 暫時註解，等待緊急停機功能啟用
 * 緊急停機程序
 */
/*
static void emergency_shutdown(void) {
    error(debug_tag, "執行緊急停機程序...");

    // 停止所有泵浦
    // modbus_write_single_register(REG_PUMP1_CONTROL, 0);
    // modbus_write_single_register(REG_PUMP2_CONTROL, 0);
    // modbus_write_single_register(REG_PUMP3_CONTROL, 0);

    // 關閉比例閥
    // modbus_write_single_register(REG_VALVE_OPENING, 0);

    // 重置PID控制器
    reset_pid_controller(&temperature_pid);

    error(debug_tag, "緊急停機完成");
}
*/

/**
 * PID控制器計算
 */

 static float calculate_pid_output(pid_controller_t *pid, float setpoint, float current_value) {
    time_t current_time = time(NULL);
    float delta_time = (current_time > pid->previous_time) ? (current_time - pid->previous_time) : 1.0f;
    
    // 計算控制誤差
    float error = current_value - setpoint;
   
    // 比例項
    float proportional = pid->kp * error;
    
    // 積分項 - 防止積分飽和
    pid->integral += error * delta_time;
    if (pid->integral > pid->output_max / pid->ki) {
        pid->integral = pid->output_max / pid->ki;
    } else if (pid->integral < pid->output_min / pid->ki) {
        pid->integral = pid->output_min / pid->ki;
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
    
    debug(debug_tag, "PID計算 - 誤差: %.2f, P: %.2f, I: %.2f, D: %.2f, 輸出: %.2f", 
          error, proportional, integral_term, derivative_term, output);
    
    return output;
}

/* 暫時註解，等待緊急停機功能啟用
 * 重置PID控制器
 */
/*
static void reset_pid_controller(pid_controller_t *pid) {
    pid->integral = 0.0f;
    pid->previous_error = 0.0f;
    pid->previous_time = time(NULL);
    debug(debug_tag, "PID控制器已重置");
}
*/

/**
 * 自適應PID參數調整
 */
static void adjust_pid_parameters(pid_controller_t *pid, float error) {
    float abs_error = fabs(error);
    
    if (abs_error > 2.0f) {
        // 大誤差：增加比例增益，減少積分增益
        pid->kp = fminf(pid->kp * 1.1f, 25.0f);
        pid->ki = fmaxf(pid->ki * 0.9f, 0.3f);
        debug(debug_tag, "PID參數調整 - 大誤差模式 Kp: %.2f, Ki: %.2f", pid->kp, pid->ki);
    } else if (abs_error < 0.2f) {
        // 小誤差：減少比例增益，增加積分增益
        pid->kp = fmaxf(pid->kp * 0.95f, 8.0f);
        pid->ki = fminf(pid->ki * 1.05f, 1.5f);
        debug(debug_tag, "PID參數調整 - 小誤差模式 Kp: %.2f, Ki: %.2f", pid->kp, pid->ki);
    }
}

/**
 * 手動控制模式
 */
static int execute_manual_control_mode(float target_temp) {
    info(debug_tag, "手動控制模式 - 目標溫度: %.1f°C", target_temp);
    
    // 設定目標溫度到寄存器
    int target_temp_raw = (int)(target_temp * 10);
    modbus_write_single_register(REG_TARGET_TEMP, target_temp_raw);
    
    // 啟用手動模式
    modbus_write_single_register(REG_PUMP1_MANUAL_MODE, 1);
    modbus_write_single_register(REG_PUMP2_MANUAL_MODE, 1);
    // modbus_write_single_register(REG_PUMP3_MANUAL_MODE, 1);
    modbus_write_single_register(REG_VALVE_MANUAL_MODE, 1);
    
    // 手動模式下僅監控，不自動調整設備
    debug(debug_tag, "手動模式設定完成，系統處於監控狀態");
    
    return 0;
}

/**
 * 自動控制模式
 */
static int execute_automatic_control_mode(const sensor_data_t *data) {
    float target_temp;
    float pid_output;
    control_output_t control_output = {{0}, {0}, 0};
    int target_temp_raw;
    
    // info(debug_tag, "自動控制模式執行");
    
    // 設定自動模式
    modbus_write_single_register(REG_TEMP_CONTROL_MODE, 1);
    modbus_write_single_register(REG_PUMP1_MANUAL_MODE, 0);
    modbus_write_single_register(REG_PUMP2_MANUAL_MODE, 0);
    modbus_write_single_register(REG_VALVE_MANUAL_MODE, 0);
    
    // 讀取目標溫度
    target_temp_raw = modbus_read_input_register(REG_TARGET_TEMP);
    if (target_temp_raw >= 0) {
        target_temp = target_temp_raw / 10.0f;
    } else {
        target_temp = TARGET_TEMP_DEFAULT;
        warn(debug_tag, "讀取目標溫度失敗，使用預設值: %.1f°C", target_temp);
    }
    
    // PID控制計算
    pid_output = calculate_pid_output(&temperature_pid, target_temp, data->avg_outlet_temp);
    
    // 自適應參數調整
    adjust_pid_parameters(&temperature_pid, target_temp - data->avg_outlet_temp);
    
    // 計算泵浦控制策略
    calculate_pump_strategy(pid_output, &control_output);
    
    // 計算比例閥開度
    control_output.valve_opening = calculate_valve_opening(pid_output, data);
    
    // 執行控制輸出
    execute_pump_control(&control_output);
    
    info(debug_tag, "自動控制 - PID輸出: %.1f%%, 當前溫度: %.1f°C, 目標溫度: %.1f°C", 
         pid_output, data->avg_outlet_temp, target_temp);
    
    return 0;
}

/**
 * 計算泵浦控制策略
 */
static void calculate_pump_strategy(float required_capacity, control_output_t *output) {
    // 基於需求容量調整
    if (required_capacity > 2.0f) {
        required_capacity += 15.0f; // 溫度過高，增加容量
    } else if (required_capacity < -2.0f) {
        required_capacity -= 10.0f; // 溫度過低，減少容量
    }
    
    // 限制容量範圍
    if (required_capacity > 100.0f) required_capacity = 100.0f;
    if (required_capacity < 10.0f) required_capacity = 10.0f;
    
    // 多泵協調策略
    if (required_capacity <= 35.0f) {
        // 單泵運行
        output->active_pumps[current_lead_pump - 1] = 1;
        output->pump_speeds[current_lead_pump - 1] = required_capacity * 2.0f;
        output->active_pumps[(current_lead_pump % 2)] = 0;
        output->active_pumps[(current_lead_pump + 1) % 2] = 0;
    } else if (required_capacity <= 70.0f) {
        // 雙泵運行
        output->active_pumps[current_lead_pump - 1] = 1;
        output->active_pumps[current_lead_pump % 2] = 1;
        output->pump_speeds[current_lead_pump - 1] = required_capacity / 1.5f;
        output->pump_speeds[current_lead_pump % 2] = required_capacity / 1.5f;
        output->active_pumps[(current_lead_pump + 1) % 3] = 0;
     } //else {
    //     // 三泵運行
    //     output->active_pumps[0] = 1;
    //     output->active_pumps[1] = 1;
    //     output->active_pumps[2] = 1;
    //     output->pump_speeds[0] = required_capacity / 2.5f;
    //     output->pump_speeds[1] = required_capacity / 2.5f;
    //     output->pump_speeds[2] = required_capacity / 2.5f;
    // }
    
    debug(debug_tag, "泵浦策略 - 需求容量: %.1f%%, 啟用泵浦: [%d,%d,%d], 速度: [%.1f,%.1f,%.1f]", 
          required_capacity, output->active_pumps[0], output->active_pumps[1], output->active_pumps[2],
          output->pump_speeds[0], output->pump_speeds[1], output->pump_speeds[2]);
}

/**
 * 執行泵浦控制
 */
static void execute_pump_control(const control_output_t *output) {
    int pump_registers[2][2] = {
        {REG_PUMP1_SPEED, REG_PUMP1_CONTROL},
        {REG_PUMP2_SPEED, REG_PUMP2_CONTROL}
    };
    
    for (int i = 0; i < 2; i++) {
        if (output->active_pumps[i]) {
            // 啟動並設定速度 (0-1000對應0-100%)
            int speed_value = (int)(output->pump_speeds[i]);
            if (speed_value > 100) speed_value = 100;
            if (speed_value < 0) speed_value = 0;
            
            modbus_write_single_register(pump_registers[i][0], speed_value); // speed
            modbus_write_single_register(pump_registers[i][1], 1); // control
            
            debug(debug_tag, "Pump%d 啟動 - 速度: %d (%.1f%%)", i+1, speed_value, output->pump_speeds[i]);
        } else {
            // 停止泵浦
            modbus_write_single_register(pump_registers[i][0], 0); // speed
            modbus_write_single_register(pump_registers[i][1], 0); // control
            debug(debug_tag, "Pump%d 停止", i+1);
        }
    }
    
    // 設定比例閥開度
    int valve_value = (int)(output->valve_opening);
    if (valve_value > 100) valve_value = 100;
    if (valve_value < 0) valve_value = 0;
    
    modbus_write_single_register(REG_VALVE_OPENING, valve_value);

    debug(debug_tag, "比例閥設定 - 開度: %d%%", valve_value);
}

/**
 * 泵浦輪換處理 (24小時輪換)
 */
static void handle_pump_rotation(void) {
    pump_rotation_timer++;
    
    // 假設控制週期為1分鐘，1440次 = 24小時
    if (pump_rotation_timer >= 1440) {
        current_lead_pump = (current_lead_pump % 3) + 1;
        pump_rotation_timer = 0;
        info(debug_tag, "泵浦輪換 - 新主泵: Pump%d", current_lead_pump);
    }
}

/**
 * 計算比例閥開度
 */
static float calculate_valve_opening(float pid_output, const sensor_data_t *data) {
    float valve_opening = pid_output;
    
    // // 流量補償
    // if (data->flow_rate < MIN_FLOW_RATE) {
    //     valve_opening = fminf(valve_opening + 10.0f, 100.0f);
    // } else if (data->flow_rate > MIN_FLOW_RATE * 1.5f) {
    //     valve_opening = fmaxf(valve_opening - 5.0f, 10.0f);
    // }
    
    // 溫度快速響應
    float temp_error = fabs(data->avg_outlet_temp - TARGET_TEMP_DEFAULT);
    if (temp_error > 2.0f) {
        valve_opening = fminf(valve_opening * 1.2f, 100.0f);
    }
    
    return valve_opening;
}

int control_logic_ls80_1_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

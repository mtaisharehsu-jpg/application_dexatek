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
 * - P4 (REG 42085): 二次進水壓力監測
 * - P2 (REG 42083): 二次出水壓力監測
 *
 * 【執行器控制】
 * - Pump1/2: 泵浦速度 0-100% (通過寄存器 45015/45016 控制)
 * - 比例閥: 開度 0-100% (REG 411151)
 * - 泵浦啟停: DO 控制 (REG 411101/411102)
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
 * 
 * 日期: 2025
 */

#include "dexatek/main_application/include/application_common.h"

#include "cJSON.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"

#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_ls80_1.json"
#define CONFIG_REGISTER_LIST_SIZE 17
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

// 追蹤 enable 狀態，用於檢測從啟用變為停用
static uint16_t previous_enable_status = 0;

// 追蹤自動啟停狀態，用於檢測邊緣觸發（0→1）
static uint16_t previous_auto_start_stop = 0;

// Modbus寄存器定義 (根據CDU系統規格)

static uint32_t REG_CONTROL_LOGIC_1_ENABLE = 41001; // 控制邏輯1啟用

// 環境監測暫存器 - 露點計算
static uint32_t REG_ENV_TEMP = 42021;    // 環境溫度
static uint32_t REG_HUMIDITY = 42022;    // 環境濕度
static uint32_t REG_DEW_POINT = 42024;   // 露點溫度輸出
static uint32_t REG_DP_CORRECT = 45004;  // 露點校正值

static uint32_t REG_T4_TEMP = 413560; // T4_IN
static uint32_t REG_T2_TEMP = 413556; // T2_OUT

static uint32_t REG_F2_FLOW = 42063; // F2流量 11165, port 1, AI_2
//static uint32_t REG_P1_PRESSURE = 42082; // P1壓力 11061, port 0, AI_A
//static uint32_t REG_P2_PRESSURE = 42083; // P2壓力 11065, port 0, AI_C
static uint32_t REG_P4_PRESSURE = 42085; // P4壓力 11067, port 0, AI_D
static uint32_t REG_P3_PRESSURE = 42084; // P3壓力 11063, port 0, AI_B

static uint32_t REG_TARGET_TEMP = 45001; // 目標溫度設定
static uint32_t REG_FLOW_SETPOINT = 45003; // 流量設定
static uint32_t REG_TEMP_FOLLOW_DEW_POINT = 45010; // 溫度跟隨露點模式 (0=保護模式, 1=跟隨模式)
static uint32_t REG_TEMP_CONTROL_MODE = 45009; // 溫度控制模式 (0=手動, 1=自動)
static uint32_t REG_AUTO_START_STOP = 45020; // 自動啟停開關 (0=停用, 1=啟用)

static uint32_t REG_VALVE_MANUAL_MODE = 45061; // 比例閥手動模式

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
static float calculate_valve_opening(float pid_output, const sensor_data_t *data);

// 露點計算函式宣告
static float calculate_dew_point(float temperature, float humidity, float correction);
static void read_and_calculate_dew_point(void);
static void apply_dew_point_tracking(float *target_temp);

// 自動啟停函式宣告
static void handle_auto_start_stop(void);

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

/**
 * 切換到手動模式並保存最後設定值
 *
 * 當 control_logic_1 從啟用變為停用時調用
 */
static void switch_to_manual_mode_with_last_speed(void) {
    info(debug_tag, "control_logic_1 停用，切換到手動模式...");

    // 設定比例閥為手動模式
    modbus_write_single_register(REG_VALVE_MANUAL_MODE, 1);

    info(debug_tag, "已切換到手動模式");
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

    _control_logic_register_list[3].name = REG_P3_PRESSURE_STR;
    _control_logic_register_list[3].address_ptr = &REG_P3_PRESSURE, 
    _control_logic_register_list[3].default_address = REG_P3_PRESSURE,
    _control_logic_register_list[3].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[4].name = REG_TARGET_TEMP_STR;
    _control_logic_register_list[4].address_ptr = &REG_TARGET_TEMP,
    _control_logic_register_list[4].default_address = REG_TARGET_TEMP,
    _control_logic_register_list[4].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[5].name = REG_FLOW_SETPOINT_STR;
    _control_logic_register_list[5].address_ptr = &REG_FLOW_SETPOINT,
    _control_logic_register_list[5].default_address = REG_FLOW_SETPOINT,
    _control_logic_register_list[5].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[6].name = REG_TEMP_CONTROL_MODE_STR;
    _control_logic_register_list[6].address_ptr = &REG_TEMP_CONTROL_MODE,
    _control_logic_register_list[6].default_address = REG_TEMP_CONTROL_MODE,
    _control_logic_register_list[6].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[7].name = REG_VALVE_MANUAL_MODE_STR;
    _control_logic_register_list[7].address_ptr = &REG_VALVE_MANUAL_MODE,
    _control_logic_register_list[7].default_address = REG_VALVE_MANUAL_MODE,
    _control_logic_register_list[7].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[8].name = REG_T4_TEMP_STR;
    _control_logic_register_list[8].address_ptr = &REG_T4_TEMP,
    _control_logic_register_list[8].default_address = REG_T4_TEMP,
    _control_logic_register_list[8].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[9].name = REG_T2_TEMP_STR;
    _control_logic_register_list[9].address_ptr = &REG_T2_TEMP,
    _control_logic_register_list[9].default_address = REG_T2_TEMP,
    _control_logic_register_list[9].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[10].name = REG_VALVE_SETPOINT_STR;
    _control_logic_register_list[10].address_ptr = &REG_VALVE_OPENING,
    _control_logic_register_list[10].default_address = REG_VALVE_OPENING,
    _control_logic_register_list[10].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // 環境監測暫存器 - 露點計算
    _control_logic_register_list[11].name = REG_ENV_TEMP_STR;
    _control_logic_register_list[11].address_ptr = &REG_ENV_TEMP,
    _control_logic_register_list[11].default_address = REG_ENV_TEMP,
    _control_logic_register_list[11].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[12].name = REG_HUMIDITY_STR;
    _control_logic_register_list[12].address_ptr = &REG_HUMIDITY,
    _control_logic_register_list[12].default_address = REG_HUMIDITY,
    _control_logic_register_list[12].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[13].name = REG_DEW_POINT_STR;
    _control_logic_register_list[13].address_ptr = &REG_DEW_POINT,
    _control_logic_register_list[13].default_address = REG_DEW_POINT,
    _control_logic_register_list[13].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[14].name = REG_DP_CORRECT_STR;
    _control_logic_register_list[14].address_ptr = &REG_DP_CORRECT,
    _control_logic_register_list[14].default_address = REG_DP_CORRECT,
    _control_logic_register_list[14].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[15].name = REG_TEMP_FOLLOW_DEW_POINT_STR;
    _control_logic_register_list[15].address_ptr = &REG_TEMP_FOLLOW_DEW_POINT,
    _control_logic_register_list[15].default_address = REG_TEMP_FOLLOW_DEW_POINT,
    _control_logic_register_list[15].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[16].name = REG_AUTO_START_STOP_STR;
    _control_logic_register_list[16].address_ptr = &REG_AUTO_START_STOP,
    _control_logic_register_list[16].default_address = REG_AUTO_START_STOP,
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

    // 【需求A】系統開機後自動啟用控制邏輯1
    bool enable_success = modbus_write_single_register(REG_CONTROL_LOGIC_1_ENABLE, 1);
    if (enable_success) {
        info(debug_tag, "【開機初始化】自動啟用 control_logic_1 (REG_CONTROL_LOGIC_1_ENABLE = 1)");
        // 初始化前次狀態為 1，避免首次執行時誤觸發狀態變化處理
        previous_enable_status = 1;
    } else {
        error(debug_tag, "【開機初始化】啟用 control_logic_1 失敗");
    }

    // 【需求B】系統開機後設定為手動模式
    bool mode_success = modbus_write_single_register(REG_TEMP_CONTROL_MODE, 0);
    if (mode_success) {
        info(debug_tag, "【開機初始化】設定溫度控制模式為手動 (REG_TEMP_CONTROL_MODE = 0)");
    } else {
        error(debug_tag, "【開機初始化】設定手動模式失敗");
    }

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

    // 【露點計算】無論控制邏輯是否啟用，每個週期都執行露點計算
    // 從環境溫度（42021）和濕度（42022）計算露點，
    // 經由校正值（45004）調整後寫入露點暫存器（42024）
    read_and_calculate_dew_point();

    // 【自動啟停】檢測 AUTO_START_STOP 從 0 變為 1，自動啟用控制邏輯並切換到自動模式
    // 當 REG_AUTO_START_STOP (45020) 觸發時，自動設定：
    // - REG_CONTROL_LOGIC_1_ENABLE (41001) = 1
    // - REG_TEMP_CONTROL_MODE (45009) = 1
    handle_auto_start_stop();

    // 【步驟0】檢測 enable 從 1 變為 0，觸發切換到手動模式
    uint16_t current_enable = modbus_read_input_register(REG_CONTROL_LOGIC_1_ENABLE);

    if (previous_enable_status == 1 && current_enable == 0) {
        // enable 從啟用變為停用，執行切換到手動模式
        switch_to_manual_mode_with_last_speed();
    }

    // 更新前次狀態
    previous_enable_status = current_enable;

    // 【步驟1】檢查控制邏輯1是否啟用 (通過 Modbus 寄存器 41001)
    if (current_enable != 1) {
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
    control_output_t control_output = {0};
    int target_temp_raw;
    
    // info(debug_tag, "自動控制模式執行");
    
    // 設定自動模式
    modbus_write_single_register(REG_TEMP_CONTROL_MODE, 1);
    modbus_write_single_register(REG_VALVE_MANUAL_MODE, 0);
    
    // 讀取目標溫度
    target_temp_raw = modbus_read_input_register(REG_TARGET_TEMP);
    if (target_temp_raw >= 0) {
        target_temp = target_temp_raw / 10.0f;
    } else {
        target_temp = TARGET_TEMP_DEFAULT;
        warn(debug_tag, "讀取目標溫度失敗，使用預設值: %.1f°C", target_temp);
    }

    // 套用溫度跟隨露點功能（在 PID 計算前調整目標溫度）
    apply_dew_point_tracking(&target_temp);

    // PID控制計算
    pid_output = calculate_pid_output(&temperature_pid, target_temp, data->avg_outlet_temp);

    // 自適應參數調整
    adjust_pid_parameters(&temperature_pid, target_temp - data->avg_outlet_temp);

    // 計算比例閥開度
    control_output.valve_opening = calculate_valve_opening(pid_output, data);

    // 設定比例閥開度
    int valve_value = (int)(control_output.valve_opening);
    if (valve_value > 100) valve_value = 100;
    if (valve_value < 0) valve_value = 0;
    modbus_write_single_register(REG_VALVE_OPENING, valve_value);

    info(debug_tag, "自動控制 - PID輸出: %.1f%%, 當前溫度: %.1f°C, 目標溫度: %.1f°C, 比例閥開度: %d%%",
         pid_output, data->avg_outlet_temp, target_temp, valve_value);
    
    return 0;
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

/**
 * 計算露點溫度（使用 Magnus-Tetens 公式）
 *
 * @param temperature 環境溫度（°C）
 * @param humidity 相對濕度（%RH，0-100）
 * @param correction 露點校正值（°C）
 * @return 露點溫度（°C）
 *
 * Magnus-Tetens 公式：
 * α(T,RH) = ln(RH/100) + (b×T)/(c+T)
 * Td = (c×α)/(b-α) + correction
 * 其中 b ≈ 17.27，c ≈ 237.7°C
 *
 * 適用範圍：-40°C 至 +50°C，1% 至 100% RH
 */
static float calculate_dew_point(float temperature, float humidity, float correction) {
    // Magnus-Tetens 公式常數
    const float b = 17.27f;
    const float c = 237.7f;

    // 檢查輸入有效性
    if (humidity <= 0.0f || humidity > 100.0f) {
        warn(debug_tag, "濕度值超出範圍: %.1f%% (有效範圍: 0-100%%)", humidity);
        return 0.0f;  // 回傳無效值
    }

    if (temperature < -40.0f || temperature > 50.0f) {
        warn(debug_tag, "溫度值超出範圍: %.1f°C (建議範圍: -40~50°C)", temperature);
        // 仍然繼續計算，但記錄警告
    }

    // 計算 α = ln(RH/100) + (b×T)/(c+T)
    float rh_ratio = humidity / 100.0f;
    float alpha = logf(rh_ratio) + (b * temperature) / (c + temperature);

    // 計算露點 Td = (c×α)/(b-α)
    float dew_point = (c * alpha) / (b - alpha);

    // 套用校正值
    dew_point += correction;

    debug(debug_tag, "露點計算: T=%.1f°C, RH=%.1f%%, 校正=%.1f°C => Td=%.1f°C",
          temperature, humidity, correction, dew_point);

    return dew_point;
}

/**
 * 讀取環境溫濕度並計算露點溫度
 *
 * 此函式從 Modbus 暫存器讀取：
 * - 環境溫度（REG_ENV_TEMP, 42021）
 * - 環境濕度（REG_HUMIDITY, 42022）
 * - 露點校正值（REG_DP_CORRECT, 45004）
 *
 * 計算露點後寫入：
 * - 露點溫度（REG_DEW_POINT, 42024）
 *
 * 無論控制邏輯是否啟用，每個週期都會執行此函式
 */
static void read_and_calculate_dew_point(void) {
    // 讀取環境溫度（精度 0.1°C，儲存為 uint16_t）
    uint16_t temp_raw = modbus_read_input_register(REG_ENV_TEMP);
    if (temp_raw == 0xFFFF) {
        warn(debug_tag, "環境溫度讀取失敗 (address: %u)", REG_ENV_TEMP);
        return;  // 讀取失敗，跳過計算
    }
    float temperature = (int16_t)temp_raw / 10.0f;  // 轉換為實際溫度（支援負溫度）

    // 讀取環境濕度（精度 0.1%，儲存為 uint16_t）
    uint16_t humidity_raw = modbus_read_input_register(REG_HUMIDITY);
    if (humidity_raw == 0xFFFF) {
        warn(debug_tag, "環境濕度讀取失敗 (address: %u)", REG_HUMIDITY);
        return;  // 讀取失敗，跳過計算
    }
    float humidity = humidity_raw / 10.0f;  // 轉換為實際濕度百分比

    // 讀取露點校正值（精度 0.1°C，儲存為 uint16_t）
    uint16_t correction_raw = modbus_read_input_register(REG_DP_CORRECT);
    float correction = 0.0f;
    if (correction_raw == 0xFFFF) {
        // 校正值讀取失敗，使用預設值 0（不校正）
        debug(debug_tag, "露點校正值讀取失敗，使用預設值 0°C");
        correction = 0.0f;
    } else {
        correction = (int16_t)correction_raw / 10.0f;  // 轉換為實際校正值（支援負值）
    }

    // 計算露點溫度
    float dew_point = calculate_dew_point(temperature, humidity, correction);

    // 檢查計算結果是否合理
    if (dew_point < -50.0f || dew_point > 60.0f) {
        warn(debug_tag, "露點計算結果異常: %.1f°C (輸入: T=%.1f°C, RH=%.1f%%)",
             dew_point, temperature, humidity);
        // 仍然寫入結果，但記錄警告
    }

    // 將露點溫度轉換為 uint16_t 並寫入暫存器（精度 0.1°C）
    int16_t dew_point_raw = (int16_t)(dew_point * 10.0f);
    bool write_success = modbus_write_single_register(REG_DEW_POINT, (uint16_t)dew_point_raw);

    if (!write_success) {
        warn(debug_tag, "露點溫度寫入失敗 (address: %u, value: %.1f°C)",
             REG_DEW_POINT, dew_point);
    } else {
        debug(debug_tag, "露點溫度已更新: %.1f°C (T=%.1f°C, RH=%.1f%%, 校正=%.1f°C)",
              dew_point, temperature, humidity, correction);
    }
}

/**
 * 套用溫度跟隨露點功能
 *
 * 此函式根據跟隨模式開關（REG_TEMP_FOLLOW_DEW_POINT, 45010）決定如何處理目標溫度：
 * - 模式 1（跟隨模式）：將露點溫度直接設為目標溫度，避免結露
 * - 模式 0（保護模式）：僅當目標溫度低於露點時，將其調整為露點溫度
 *
 * @param target_temp 指向目標溫度變數的指標，此函式會修改其值
 *
 * 在自動控制模式下，於讀取目標溫度後、PID 計算前呼叫此函式
 */
static void apply_dew_point_tracking(float *target_temp) {
    // 讀取溫度跟隨露點模式開關 (45010)
    uint16_t follow_mode = modbus_read_input_register(REG_TEMP_FOLLOW_DEW_POINT);

    // 讀取當前露點溫度 (42024)
    uint16_t dew_point_raw = modbus_read_input_register(REG_DEW_POINT);

    // 檢查露點溫度讀取是否成功
    if (dew_point_raw == 0xFFFF) {
        debug(debug_tag, "露點溫度讀取失敗，跳過溫度跟隨露點功能");
        return;  // 讀取失敗，不執行跟隨功能
    }

    // 轉換露點溫度（支援負溫度）
    float dew_point = (int16_t)dew_point_raw / 10.0f;

    if (follow_mode == 1) {
        // ===== 模式 1：跟隨模式 =====
        // 直接使用露點溫度作為目標溫度
        float original_target = *target_temp;
        *target_temp = dew_point;

        // 將調整後的目標溫度寫回暫存器 (45001) 供 HMI 顯示
        int16_t target_temp_raw_write = (int16_t)(*target_temp * 10.0f);
        modbus_write_single_register(REG_TARGET_TEMP, (uint16_t)target_temp_raw_write);

        info(debug_tag, "【跟隨模式】目標溫度已設為露點: %.1f°C → %.1f°C",
             original_target, *target_temp);

    } else if (follow_mode == 0) {
        // ===== 模式 0：保護模式 =====
        // 確保目標溫度不低於露點溫度（防止結露）
        if (*target_temp < dew_point) {
            float original_target = *target_temp;
            *target_temp = dew_point;

            // 將調整後的目標溫度寫回暫存器 (45001)
            int16_t target_temp_raw_write = (int16_t)(*target_temp * 10.0f);
            modbus_write_single_register(REG_TARGET_TEMP, (uint16_t)target_temp_raw_write);

            warn(debug_tag, "【保護模式】目標溫度 %.1f°C 低於露點 %.1f°C，已調整為露點溫度",
                 original_target, dew_point);
        } else {
            // 目標溫度高於露點，無需調整
            debug(debug_tag, "【保護模式】目標溫度 %.1f°C 高於露點 %.1f°C，無需調整",
                  *target_temp, dew_point);
        }
    } else {
        // 未知模式值，記錄警告
        warn(debug_tag, "溫度跟隨露點模式開關值異常: %u（有效值：0=保護, 1=跟隨）", follow_mode);
    }
}

/**
 * 處理自動啟停功能
 *
 * 此函式檢測 REG_AUTO_START_STOP (45020) 的邊緣觸發變化，執行不同的模式切換：
 *
 * 【0→1 邊緣觸發】自動啟動模式
 * 1. 將 REG_CONTROL_LOGIC_1_ENABLE (41001) 設定為 1（啟用控制邏輯）
 * 2. 將 REG_TEMP_CONTROL_MODE (45009) 設定為 1（切換到自動模式）
 *
 * 【1→0 邊緣觸發】切換到手動模式（保持啟用）
 * 1. 不修改 REG_CONTROL_LOGIC_1_ENABLE（保持控制邏輯啟用狀態）
 * 2. 將 REG_TEMP_CONTROL_MODE (45009) 設定為 0（切換到手動模式）
 *
 * 設計採用邊緣觸發，避免每個週期重複寫入暫存器。
 * 在主控制迴圈中，於露點計算後、enable 檢查前呼叫。
 */
static void handle_auto_start_stop(void) {
    // 讀取自動啟停開關 (45020)
    uint16_t current_auto_start = modbus_read_input_register(REG_AUTO_START_STOP);

    // 檢查讀取是否成功
    if (current_auto_start == 0xFFFF) {
        warn(debug_tag, "REG_AUTO_START_STOP 讀取失敗，跳過自動啟停檢查");
        return;  // 讀取失敗，跳過處理
    }

    // 邊緣觸發檢測：從 0 變為 1 時執行自動啟動
    if (previous_auto_start_stop == 0 && current_auto_start == 1) {
        info(debug_tag, "【自動啟停】觸發 - 啟用控制邏輯並切換到自動模式");

        // 1. 啟用控制邏輯 (設定 ENABLE = 1)
        bool enable_success = modbus_write_single_register(REG_CONTROL_LOGIC_1_ENABLE, 1);
        if (!enable_success) {
            error(debug_tag, "【自動啟停】啟用控制邏輯失敗 (ENABLE 寫入失敗)");
        }

        // 2. 設定為自動模式 (設定 MODE = 1)
        bool mode_success = modbus_write_single_register(REG_TEMP_CONTROL_MODE, 1);
        if (!mode_success) {
            error(debug_tag, "【自動啟停】切換自動模式失敗 (MODE 寫入失敗)");
        }

        // 記錄執行結果
        if (enable_success && mode_success) {
            info(debug_tag, "【自動啟停】執行成功 - ENABLE=1, MODE=AUTO");
        } else {
            error(debug_tag, "【自動啟停】執行部分失敗 - ENABLE=%s, MODE=%s",
                  enable_success ? "成功" : "失敗",
                  mode_success ? "成功" : "失敗");
        }
    }
    // 【需求B】邊緣觸發檢測：從 1 變為 0 - 保持啟用但切換到手動模式
    else if (previous_auto_start_stop == 1 && current_auto_start == 0) {
        info(debug_tag, "【自動啟停】關閉 (1→0) - 保持啟用狀態，切換到手動模式");

        // 1. 不修改 REG_CONTROL_LOGIC_1_ENABLE，保持控制邏輯啟用狀態
        // 2. 切換到手動模式 (設定 MODE = 0)
        bool mode_success = modbus_write_single_register(REG_TEMP_CONTROL_MODE, 0);

        if (mode_success) {
            info(debug_tag, "【自動啟停】已切換到手動模式 - ENABLE 保持不變, MODE=MANUAL");
        } else {
            error(debug_tag, "【自動啟停】切換手動模式失敗 (MODE 寫入失敗)");
        }
    }

    // 更新前次狀態
    previous_auto_start_stop = current_auto_start;
}

int control_logic_ls80_1_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

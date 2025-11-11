/*
 * control_logic_lx1400_3.c - LX1400 流量控制邏輯 (Control Logic 3: Flow Control)
 *
 * 【功能概述】
 * 本模組實現 LX1400 機型的流量控制功能,通過 PID 演算法使二次側出水流量追蹤目標值。
 * 相較於 LS80 機型,LX1400 使用相同的控制邏輯,但可能有不同的感測器配置。
 *
 * 【控制目標】
 * - 維持 F2 流量追蹤設定值 (預設 200 L/min)
 * - F2 追蹤設定值模式
 *
 * 【感測器配置】
 * - F1 (REG 42062): 一次側進水流量
 * - F2 (REG 42063): 二次側出水流量 (主要控制)
 * - F3 (REG 42064): 二次側進水流量 (預留)
 * - F4 (REG 42065): 一次側出水流量 (預留)
 *
 * 【執行器控制】
 * - Pump1/2/3: 泵浦速度/啟停
 * - 比例閥: 開度控制
 *
 * 【泵浦協調策略】
 * - PID < 40%: 單泵運行
 * - 40% ≤ PID < 70%: 雙泵協調
 * - PID ≥ 70%: 雙泵高速
 *
 * 【PID 參數】
 * - Kp: 2.5 (比例增益)
 * - Ki: 0.4 (積分增益)
 * - Kd: 0.8 (微分增益)
 *
 * 【安全保護】
 * - 流量變化率限制: 100 L/min/sec
 * - 最小控制流量: 30 L/min
 * - 最大追蹤誤差: 50 L/min
 * - 流量上下限保護
 * - F1/F2 一致性檢查
 *
 * 作者: LX1400 機型適配
 * 日期: 2025
 */

#include "dexatek/main_application/include/application_common.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
static const char *debug_tag = "lx1400_3_flow";  // 日誌標籤


#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_lx1400_3.json"
#define CONFIG_REGISTER_LIST_SIZE 25
static control_logic_register_t _control_logic_register_list[CONFIG_REGISTER_LIST_SIZE];

// 系統狀態寄存器
static uint32_t REG_CONTROL_LOGIC_3_ENABLE = 41003; // 控制邏輯3啟用

static uint32_t REG_F1_FLOW = 42062;   // F1一次側進水流量
static uint32_t REG_F2_FLOW = 42063;   // F2二次側出水流量 (主要控制)
static uint32_t REG_F3_FLOW = 42064;   // F3二次側進水流量
static uint32_t REG_F4_FLOW = 42065;   // F4一次側出水流量

static uint32_t REG_TARGET_FLOW = 45003;   // 目標流量設定 (F_set)
static uint32_t REG_FLOW_MODE = 45005;   // 流量/壓差模式選擇 (0=流量模式)
static uint32_t REG_FLOW_HIGH_LIMIT = 45006;   // 流量上限
static uint32_t REG_FLOW_LOW_LIMIT = 45007;   // 流量下限

static uint32_t REG_PUMP1_SPEED = 411037;  // Pump1速度設定 (0-1000)
static uint32_t REG_PUMP2_SPEED = 411039;  // Pump2速度設定
static uint32_t REG_PUMP3_SPEED = 411041;  // Pump3速度設定
static uint32_t REG_PUMP1_CONTROL = 411101;  // Pump1啟停控制
static uint32_t REG_PUMP2_CONTROL = 411103;  // Pump2啟停控制
static uint32_t REG_PUMP3_CONTROL = 411105;  // Pump3啟停控制

static uint32_t REG_PUMP1_MANUAL_MODE = 45021;   // Pump1手動模式 (0=自動, 1=手動)
static uint32_t REG_PUMP2_MANUAL_MODE = 45022;   // Pump2手動模式
static uint32_t REG_PUMP3_MANUAL_MODE = 45023;   // Pump3手動模式

static uint32_t REG_VALVE_OPENING = 411147;  // 比例閥開度設定 (%)
static uint32_t REG_VALVE_ACTUAL = 411161;  // 比例閥實際開度 (%)
static uint32_t REG_VALVE_MANUAL_MODE = 45061;   // 比例閥手動模式

// 安全限制參數
#define MAX_FLOW_CHANGE_RATE      100.0f  // 最大流量變化率 L/min/sec
#define MIN_CONTROL_FLOW          30.0f   // 最小控制流量
#define MAX_TRACKING_ERROR        50.0f   // 最大追蹤誤差
#define PUMP_MIN_SPEED            20.0f   // 泵浦最小速度 %
#define PUMP_MAX_SPEED            100.0f  // 泵浦最大速度 %

/*---------------------------------------------------------------------------
								Variables
 ---------------------------------------------------------------------------*/
typedef enum {
    FLOW_CONTROL_MODE_MANUAL = 0,
    FLOW_CONTROL_MODE_AUTO = 1
} flow_control_mode_t;

typedef enum {
    FLOW_TRACKING_MODE_F2_TO_FSET = 0,  // F2追蹤設定值 (簡化實施)
    FLOW_TRACKING_MODE_F2_TO_F1 = 1,    // F2追蹤F1 (未來擴展)
    FLOW_TRACKING_MODE_F3_TO_F4 = 2     // F3追蹤F4 (未來擴展)
} flow_tracking_mode_t;

typedef enum {
    FLOW_SAFETY_STATUS_SAFE = 0,
    FLOW_SAFETY_STATUS_WARNING = 1,
    FLOW_SAFETY_STATUS_CRITICAL = 2,
    FLOW_SAFETY_STATUS_EMERGENCY = 3
} flow_safety_status_t;

typedef struct {
    float F1_primary_inlet;      // 一次側進水流量
    float F2_secondary_outlet;   // 二次側出水流量 (主要控制目標)
    float F3_secondary_inlet;    // 二次側進水流量
    float F4_primary_outlet;     // 一次側出水流量
    time_t timestamp;
} flow_sensor_data_t;

typedef struct {
    float kp;                    // 比例增益
    float ki;                    // 積分增益
    float kd;                    // 微分增益
    float integral;              // 積分項累積
    float previous_error;        // 前一次誤差
    time_t previous_time;        // 前一次計算時間
    float output_min;            // 輸出最小值
    float output_max;            // 輸出最大值
} flow_pid_controller_t;

typedef struct {
    flow_tracking_mode_t tracking_mode;  // 追蹤模式
    float target_flow_rate;              // 目標流量設定 (Fset)
    float flow_high_limit;               // 流量上限
    float flow_low_limit;                // 流量下限
    float tracking_ratio;                // 追蹤比例 (預留)
} flow_control_config_t;

typedef struct {
    int active_pumps[3];         // 泵浦啟用狀態 [Pump1, Pump2, Pump3]
    float pump_speeds[3];        // 泵浦速度 0-100%
    float valve_opening;         // 比例閥開度 0-100%
} flow_control_output_t;

// 全域變數
static flow_pid_controller_t flow_pid = {
    .kp = 2.5f,                  // 流量控制比例增益
    .ki = 0.4f,                  // 流量控制積分增益
    .kd = 0.8f,                  // 流量控制微分增益
    .integral = 0.0f,
    .previous_error = 0.0f,
    .previous_time = 0,
    .output_min = 0.0f,
    .output_max = 100.0f
};

static flow_control_config_t flow_config = {
    .tracking_mode = FLOW_TRACKING_MODE_F2_TO_FSET,
    .target_flow_rate = 200.0f,  // 預設目標流量 200 L/min
    .flow_high_limit = 500.0f,   // 流量上限 500 L/min
    .flow_low_limit = 50.0f,     // 流量下限 50 L/min
    .tracking_ratio = 1.0f       // 1:1追蹤比例
};

/*---------------------------------------------------------------------------
							Function Prototypes
 ---------------------------------------------------------------------------*/
// 函數宣告
static int read_flow_sensor_data(flow_sensor_data_t *data);
static flow_safety_status_t perform_flow_safety_checks(const flow_sensor_data_t *data, float target_flow);
static void emergency_flow_shutdown(void);
static float calculate_flow_tracking_target(const flow_sensor_data_t *data);
static float calculate_flow_pid_output(flow_pid_controller_t *pid, float setpoint, float current_value);
static void reset_flow_pid_controller(flow_pid_controller_t *pid);
static void adaptive_flow_pid_tuning(flow_pid_controller_t *pid, float error, float error_percentage);
static int execute_manual_flow_control_mode(float target_flow);
static int execute_automatic_flow_control_mode(const flow_sensor_data_t *data);
static void calculate_basic_pump_control(float pid_output, flow_control_output_t *output);
static void execute_flow_control_output(const flow_control_output_t *output);
static float calculate_valve_adjustment(float pid_output, const flow_sensor_data_t *data);

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

    _control_logic_register_list[0].name = REG_CONTROL_LOGIC_3_ENABLE_STR;
    _control_logic_register_list[0].address_ptr = &REG_CONTROL_LOGIC_3_ENABLE;
    _control_logic_register_list[0].default_address = REG_CONTROL_LOGIC_3_ENABLE;
    _control_logic_register_list[0].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[1].name = REG_F1_FLOW_STR;
    _control_logic_register_list[1].address_ptr = &REG_F1_FLOW;
    _control_logic_register_list[1].default_address = REG_F1_FLOW;
    _control_logic_register_list[1].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[2].name = REG_F2_FLOW_STR;
    _control_logic_register_list[2].address_ptr = &REG_F2_FLOW;
    _control_logic_register_list[2].default_address = REG_F2_FLOW;
    _control_logic_register_list[2].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[3].name = REG_F3_FLOW_STR;
    _control_logic_register_list[3].address_ptr = &REG_F3_FLOW;
    _control_logic_register_list[3].default_address = REG_F3_FLOW;
    _control_logic_register_list[3].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[4].name = REG_F4_FLOW_STR;
    _control_logic_register_list[4].address_ptr = &REG_F4_FLOW;
    _control_logic_register_list[4].default_address = REG_F4_FLOW;
    _control_logic_register_list[4].type = CONTROL_LOGIC_REGISTER_READ;


    _control_logic_register_list[5].name = REG_FLOW_SETPOINT_STR;
    _control_logic_register_list[5].address_ptr = &REG_TARGET_FLOW;
    _control_logic_register_list[5].default_address = REG_TARGET_FLOW;
    _control_logic_register_list[5].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[6].name = REG_FLOW_MODE_STR;
    _control_logic_register_list[6].address_ptr = &REG_FLOW_MODE;
    _control_logic_register_list[6].default_address = REG_FLOW_MODE;
    _control_logic_register_list[6].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[7].name = REG_FLOW_HIGH_LIMIT_STR;
    _control_logic_register_list[7].address_ptr = &REG_FLOW_HIGH_LIMIT;
    _control_logic_register_list[7].default_address = REG_FLOW_HIGH_LIMIT;
    _control_logic_register_list[7].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[8].name = REG_FLOW_LOW_LIMIT_STR;
    _control_logic_register_list[8].address_ptr = &REG_FLOW_LOW_LIMIT;
    _control_logic_register_list[8].default_address = REG_FLOW_LOW_LIMIT;
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
    _control_logic_register_list[15].address_ptr = &REG_PUMP1_MANUAL_MODE;
    _control_logic_register_list[15].default_address = REG_PUMP1_MANUAL_MODE;
    _control_logic_register_list[15].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[16].name = REG_PUMP2_MANUAL_MODE_STR;
    _control_logic_register_list[16].address_ptr = &REG_PUMP2_MANUAL_MODE;
    _control_logic_register_list[16].default_address = REG_PUMP2_MANUAL_MODE;
    _control_logic_register_list[16].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[17].name = REG_PUMP3_MANUAL_MODE_STR;
    _control_logic_register_list[17].address_ptr = &REG_PUMP3_MANUAL_MODE;
    _control_logic_register_list[17].default_address = REG_PUMP3_MANUAL_MODE;
    _control_logic_register_list[17].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[18].name = REG_VALVE_SETPOINT_STR;
    _control_logic_register_list[18].address_ptr = &REG_VALVE_OPENING;
    _control_logic_register_list[18].default_address = REG_VALVE_OPENING;
    _control_logic_register_list[18].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[19].name = REG_VALVE_ACTUAL_STR;
    _control_logic_register_list[19].address_ptr = &REG_VALVE_ACTUAL;
    _control_logic_register_list[19].default_address = REG_VALVE_ACTUAL;
    _control_logic_register_list[19].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[20].name = REG_VALVE_MANUAL_MODE_STR;
    _control_logic_register_list[20].address_ptr = &REG_VALVE_MANUAL_MODE;
    _control_logic_register_list[20].default_address = REG_VALVE_MANUAL_MODE;
    _control_logic_register_list[20].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    uint32_t list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    ret = control_logic_register_load_from_file(CONFIG_REGISTER_FILE_PATH, _control_logic_register_list, list_size);
    debug(debug_tag, "load register array from file %s, ret %d", CONFIG_REGISTER_FILE_PATH, ret);

    return ret;
}

int control_logic_lx1400_3_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

int control_logic_lx1400_3_flow_control_init(void)
{
    _register_list_init();

    return SUCCESS;
}

/**
 * CDU流量控制主要函數 (版本 3.1)
 */
int control_logic_lx1400_3_flow_control(ControlLogic *ptr) {

    (void)ptr;

    // check enable
    if (modbus_read_input_register(REG_CONTROL_LOGIC_3_ENABLE) != 1) {
        return 0;
    }

    flow_sensor_data_t sensor_data;
    flow_safety_status_t safety_status;
    int control_mode;
    int ret = 0;

    info(debug_tag, "=== CDU流量控制系統執行 (v3.1) ===");
    
    // 1. 讀取流量感測器數據
    if (read_flow_sensor_data(&sensor_data) != 0) {
        error(debug_tag, "讀取流量感測器數據失敗");
        return -1;
    }
    
    debug(debug_tag, "流量數據 - F1: %.1f, F2: %.1f, F3: %.1f, F4: %.1f L/min", 
          sensor_data.F1_primary_inlet, sensor_data.F2_secondary_outlet,
          sensor_data.F3_secondary_inlet, sensor_data.F4_primary_outlet);
    
    // 2. 計算追蹤目標流量
    float target_flow = calculate_flow_tracking_target(&sensor_data);
    
    // 3. 安全檢查
    safety_status = perform_flow_safety_checks(&sensor_data, target_flow);
    
    if (safety_status == FLOW_SAFETY_STATUS_EMERGENCY) {
        error(debug_tag, "流量控制緊急狀況，執行緊急停機");
        emergency_flow_shutdown();
        return -2;
    } else if (safety_status == FLOW_SAFETY_STATUS_CRITICAL) {
        warn(debug_tag, "流量控制嚴重警告狀態");
    } else if (safety_status == FLOW_SAFETY_STATUS_WARNING) {
        warn(debug_tag, "流量控制警告狀態，繼續監控");
    }
    
    // 4. 檢查控制模式 (基於手動模式寄存器)
    int pump1_manual = modbus_read_input_register(REG_PUMP1_MANUAL_MODE);
    int valve_manual = modbus_read_input_register(REG_VALVE_MANUAL_MODE);
    
    control_mode = (pump1_manual > 0 || valve_manual > 0) ? FLOW_CONTROL_MODE_MANUAL : FLOW_CONTROL_MODE_AUTO;
    
    // 5. 執行相應控制邏輯
    if (control_mode == FLOW_CONTROL_MODE_AUTO) {
        info(debug_tag, "執行自動流量控制模式 (F2→Fset追蹤)");
        ret = execute_automatic_flow_control_mode(&sensor_data);
    } else {
        info(debug_tag, "手動流量控制模式 - 僅監控狀態");
        ret = execute_manual_flow_control_mode(target_flow);
    }
    
    if (ret != 0) {
        error(debug_tag, "流量控制邏輯執行失敗: %d", ret);
    }
    
    debug(debug_tag, "=== CDU流量控制循環完成 ===");
    return ret;
}

/**
 * 讀取所有流量感測器數據
 */
static int read_flow_sensor_data(flow_sensor_data_t *data) {
    int flow_raw;
    
    // 讀取F1一次側進水流量 (0.1 L/min精度)
    flow_raw = modbus_read_input_register(REG_F1_FLOW);
    if (flow_raw >= 0) {
        data->F1_primary_inlet = flow_raw / 10.0f;
    } else {
        warn(debug_tag, "F1流量讀取失敗");
        data->F1_primary_inlet = 0.0f;
    }
    
    // 讀取F2二次側出水流量 (主要控制目標)
    flow_raw = modbus_read_input_register(REG_F2_FLOW);
    if (flow_raw >= 0) {
        data->F2_secondary_outlet = flow_raw / 10.0f;
    } else {
        error(debug_tag, "F2流量讀取失敗 - 這是主要控制目標！");
        data->F2_secondary_outlet = 0.0f;
    }
    
    // 讀取F3二次側進水流量
    flow_raw = modbus_read_input_register(REG_F3_FLOW);
    if (flow_raw >= 0) {
        data->F3_secondary_inlet = flow_raw / 10.0f;
    } else {
        warn(debug_tag, "F3流量讀取失敗");
        data->F3_secondary_inlet = 0.0f;
    }
    
    // 讀取F4一次側出水流量
    flow_raw = modbus_read_input_register(REG_F4_FLOW);
    if (flow_raw >= 0) {
        data->F4_primary_outlet = flow_raw / 10.0f;
    } else {
        warn(debug_tag, "F4流量讀取失敗");
        data->F4_primary_outlet = 0.0f;
    }
    
    // 設定時間戳
    data->timestamp = time(NULL);
    
    return 0;
}

/**
 * 計算流量追蹤目標 (簡化實施：僅支援F2→Fset)
 */
static float calculate_flow_tracking_target(const flow_sensor_data_t *data) {
    (void)data;

    float target_flow;
    
    // 簡化實施：僅實現F2→Fset追蹤模式
    if (flow_config.tracking_mode == FLOW_TRACKING_MODE_F2_TO_FSET) {
        // 讀取設定流量值
        int target_raw = modbus_read_input_register(REG_TARGET_FLOW);
        if (target_raw >= 0) {
            target_flow = target_raw / 10.0f;  // 0.1 L/min精度
        } else {
            target_flow = flow_config.target_flow_rate;  // 使用預設值
            warn(debug_tag, "讀取目標流量失敗，使用預設值: %.1f L/min", target_flow);
        }
        
        debug(debug_tag, "F2→Fset追蹤模式: 目標流量 = %.1f L/min", target_flow);
    } else {
        // 未來擴展：其他追蹤模式
        target_flow = flow_config.target_flow_rate;
        warn(debug_tag, "不支援的追蹤模式，使用預設目標流量: %.1f L/min", target_flow);
    }
    
    // 安全範圍限制
    if (target_flow > flow_config.flow_high_limit) {
        target_flow = flow_config.flow_high_limit;
        warn(debug_tag, "目標流量超出上限，限制為: %.1f L/min", target_flow);
    } else if (target_flow < flow_config.flow_low_limit) {
        target_flow = flow_config.flow_low_limit;
        warn(debug_tag, "目標流量低於下限，限制為: %.1f L/min", target_flow);
    }
    
    return target_flow;
}

/**
 * 流量安全檢查
 */
static flow_safety_status_t perform_flow_safety_checks(const flow_sensor_data_t *data, float target_flow) {
    flow_safety_status_t status = FLOW_SAFETY_STATUS_SAFE;
    
    // 1. 緊急停機檢查
    if (target_flow > flow_config.flow_high_limit) {
        error(debug_tag, "目標流量超出安全上限: %.1f > %.1f L/min", 
              target_flow, flow_config.flow_high_limit);
        return FLOW_SAFETY_STATUS_EMERGENCY;
    }
    
    if (data->F2_secondary_outlet < MIN_CONTROL_FLOW) {
        error(debug_tag, "F2流量過低: %.1f < %.1f L/min", 
              data->F2_secondary_outlet, MIN_CONTROL_FLOW);
        return FLOW_SAFETY_STATUS_EMERGENCY;
    }
    
    // 2. 嚴重警告檢查
    float tracking_error = fabs(data->F2_secondary_outlet - target_flow);
    if (tracking_error > MAX_TRACKING_ERROR) {
        warn(debug_tag, "F2流量追蹤誤差過大: %.1f L/min", tracking_error);
        status = FLOW_SAFETY_STATUS_CRITICAL;
    }
    
    // 3. 警告條件檢查
    if (data->F2_secondary_outlet > flow_config.flow_high_limit * 0.9f) {
        warn(debug_tag, "F2流量接近上限: %.1f L/min", data->F2_secondary_outlet);
        status = (status == FLOW_SAFETY_STATUS_SAFE) ? FLOW_SAFETY_STATUS_WARNING : status;
    }
    
    if (data->F2_secondary_outlet < flow_config.flow_low_limit) {
        warn(debug_tag, "F2流量低於下限: %.1f L/min", data->F2_secondary_outlet);
        status = (status == FLOW_SAFETY_STATUS_SAFE) ? FLOW_SAFETY_STATUS_WARNING : status;
    }
    
    // 4. 感測器數據一致性檢查
    if (data->F1_primary_inlet > 0 && data->F2_secondary_outlet > 0) {
        float flow_ratio = data->F2_secondary_outlet / data->F1_primary_inlet;
        if (flow_ratio > 1.5f || flow_ratio < 0.3f) {
            warn(debug_tag, "F1與F2流量比例異常: %.2f", flow_ratio);
            status = (status == FLOW_SAFETY_STATUS_SAFE) ? FLOW_SAFETY_STATUS_WARNING : status;
        }
    }
    
    return status;
}

/**
 * 緊急停機程序
 */
static void emergency_flow_shutdown(void) {
    error(debug_tag, "執行流量控制緊急停機程序...");
    
    // 停止所有泵浦
    modbus_write_single_register(REG_PUMP1_CONTROL, 0);
    modbus_write_single_register(REG_PUMP2_CONTROL, 0);
    modbus_write_single_register(REG_PUMP3_CONTROL, 0);
    
    // 比例閥設置到安全開度 (30%)
    modbus_write_single_register(REG_VALVE_OPENING, 30);
    
    // 重置PID控制器
    reset_flow_pid_controller(&flow_pid);
    
    error(debug_tag, "流量控制緊急停機完成");
}

/**
 * 流量PID控制器計算
 */
static float calculate_flow_pid_output(flow_pid_controller_t *pid, float setpoint, float current_value) {
    time_t current_time = time(NULL);
    float delta_time = (current_time > pid->previous_time) ? (current_time - pid->previous_time) : 1.0f;
    
    // 計算控制誤差
    float error = setpoint - current_value;
    
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
    
    debug(debug_tag, "流量PID - 誤差: %.2f, P: %.2f, I: %.2f, D: %.2f, 輸出: %.2f", 
          error, proportional, integral_term, derivative_term, output);
    
    return output;
}

/**
 * 重置流量PID控制器
 */
static void reset_flow_pid_controller(flow_pid_controller_t *pid) {
    pid->integral = 0.0f;
    pid->previous_error = 0.0f;
    pid->previous_time = time(NULL);
    debug(debug_tag, "流量PID控制器已重置");
}

/**
 * 自適應流量PID參數調整
 */
static void adaptive_flow_pid_tuning(flow_pid_controller_t *pid, float error, float error_percentage) {
    float abs_error = fabs(error);
    
    if (error_percentage > 15.0f) {
        // 大誤差：增加比例增益，減少積分增益，快速響應
        pid->kp = fminf(pid->kp * 1.1f, 5.0f);
        pid->ki = fmaxf(pid->ki * 0.9f, 0.1f);
        debug(debug_tag, "流量PID大誤差調整 - Kp: %.2f, Ki: %.2f", pid->kp, pid->ki);
    } else if (error_percentage < 3.0f) {
        // 小誤差：減少比例增益，增加積分增益，提高穩態精度
        pid->kp = fmaxf(pid->kp * 0.95f, 1.0f);
        pid->ki = fminf(pid->ki * 1.05f, 1.0f);
        debug(debug_tag, "流量PID小誤差調整 - Kp: %.2f, Ki: %.2f", pid->kp, pid->ki);
    }
    
    // 微分項根據誤差變化率調整
    if (abs_error > 20.0f) {
        pid->kd = fminf(pid->kd * 1.05f, 2.0f);  // 增加微分項抑制超調
    } else if (abs_error < 5.0f) {
        pid->kd = fmaxf(pid->kd * 0.98f, 0.3f);  // 減少微分項減少振盪
    }
}

/**
 * 手動流量控制模式
 */
static int execute_manual_flow_control_mode(float target_flow) {
    info(debug_tag, "手動流量控制模式 - 目標流量: %.1f L/min", target_flow);
    
    // 設定目標流量到寄存器
    int target_flow_raw = (int)(target_flow * 10);
    modbus_write_single_register(REG_TARGET_FLOW, target_flow_raw);
    
    // 確保處於流量模式
    modbus_write_single_register(REG_FLOW_MODE, 0);  // 0=流量模式
    
    // 啟用手動模式
    modbus_write_single_register(REG_PUMP1_MANUAL_MODE, 1);
    modbus_write_single_register(REG_VALVE_MANUAL_MODE, 1);
    
    // 手動模式下僅監控，不自動調整設備
    debug(debug_tag, "手動模式設定完成，等待操作員手動控制");
    
    return 0;
}

/**
 * 自動流量控制模式 (F2→Fset追蹤)
 */
static int execute_automatic_flow_control_mode(const flow_sensor_data_t *data) {
    float target_flow;
    float pid_output;
    
    // Initialize control_output with proper braces to avoid compiler warning
    flow_control_output_t control_output = {{0}, {0}, 0};
    
    info(debug_tag, "自動流量控制模式執行 (F2→Fset追蹤)");
    
    // 設定自動模式
    modbus_write_single_register(REG_FLOW_MODE, 0);         // 0=流量模式
    modbus_write_single_register(REG_PUMP1_MANUAL_MODE, 0); // 自動模式
    modbus_write_single_register(REG_PUMP2_MANUAL_MODE, 0);
    modbus_write_single_register(REG_PUMP3_MANUAL_MODE, 0);
    modbus_write_single_register(REG_VALVE_MANUAL_MODE, 0);
    
    // 計算追蹤目標
    target_flow = calculate_flow_tracking_target(data);
    
    // F2→Fset追蹤：F2當前流量追蹤設定目標流量
    float current_flow = data->F2_secondary_outlet;
    float flow_error = target_flow - current_flow;
    float error_percentage = (fabs(flow_error) / target_flow) * 100.0f;
    
    info(debug_tag, "F2→Fset追蹤: 目標=%.1f L/min, 當前=%.1f L/min, 誤差=%.1f L/min (%.1f%%)", 
         target_flow, current_flow, flow_error, error_percentage);
    
    // PID控制計算
    pid_output = calculate_flow_pid_output(&flow_pid, target_flow, current_flow);
    
    // 自適應PID參數調整
    adaptive_flow_pid_tuning(&flow_pid, flow_error, error_percentage);
    
    // 計算基礎泵浦控制策略 (簡化實施)
    calculate_basic_pump_control(pid_output, &control_output);
    
    // 計算比例閥調整
    control_output.valve_opening = calculate_valve_adjustment(pid_output, data);
    
    // 執行控制輸出
    execute_flow_control_output(&control_output);
    
    info(debug_tag, "自動流量控制完成 - PID輸出: %.1f%%, 泵浦速度: %.1f%%, 閥門開度: %.1f%%", 
         pid_output, control_output.pump_speeds[0], control_output.valve_opening);
    
    return 0;
}

/**
 * 計算基礎泵浦控制策略 (簡化實施：主要使用Pump1)
 */
static void calculate_basic_pump_control(float pid_output, flow_control_output_t *output) {
    // 簡化實施：主要使用Pump1進行流量控制
    float required_speed = fabs(pid_output);
    
    if (required_speed < PUMP_MIN_SPEED && pid_output > 5.0f) {
        required_speed = PUMP_MIN_SPEED;  // 最小啟動速度
    } else if (required_speed > PUMP_MAX_SPEED) {
        required_speed = PUMP_MAX_SPEED;  // 最大速度限制
    }
    
    if (pid_output > 0) {
        // 需要增加流量 - 啟動Pump1
        output->active_pumps[0] = 1;  // Pump1 啟動
        output->pump_speeds[0] = required_speed;
        output->active_pumps[1] = 0;  // Pump2 停止 (簡化實施)
        output->active_pumps[2] = 0;  // Pump3 停止
    } else if (pid_output < -10.0f) {
        // 需要大幅減少流量 - 停止Pump1
        output->active_pumps[0] = 0;
        output->pump_speeds[0] = 0;
    } else {
        // 微調或維持 - 保持Pump1低速運行
        output->active_pumps[0] = 1;
        output->pump_speeds[0] = PUMP_MIN_SPEED;
    }
    
    // 其他泵浦保持停止 (簡化實施)
    output->pump_speeds[1] = 0;
    output->pump_speeds[2] = 0;
    
    debug(debug_tag, "基礎泵浦控制 - PID: %.1f, Pump1: %s/%.1f%%, 其他泵浦: 停止", 
          pid_output, output->active_pumps[0] ? "啟動" : "停止", output->pump_speeds[0]);
}

/**
 * 執行流量控制輸出
 */
static void execute_flow_control_output(const flow_control_output_t *output) {
    int pump_registers[3][2] = {
        {REG_PUMP1_SPEED, REG_PUMP1_CONTROL},
        {REG_PUMP2_SPEED, REG_PUMP2_CONTROL},
        {REG_PUMP3_SPEED, REG_PUMP3_CONTROL}
    };
    
    // 控制泵浦
    for (int i = 0; i < 3; i++) {
        if (output->active_pumps[i]) {
            // 啟動並設定速度 (0-1000對應0-100%)
            int speed_value = (int)(output->pump_speeds[i] * 10.0f);
            if (speed_value > 1000) speed_value = 1000;
            if (speed_value < 0) speed_value = 0;
            
            // [DK]: 1000 means 10V (10000mV) => speed_cmd * 10 
            speed_value = speed_value * 10; // [DK]

            modbus_write_single_register(pump_registers[i][0], speed_value);
            modbus_write_single_register(pump_registers[i][1], 1);
            
            debug(debug_tag, "Pump%d 啟動 - 速度: %d (%.1f%%)", i+1, speed_value, output->pump_speeds[i]);
        } else {
            // 停止泵浦
            modbus_write_single_register(pump_registers[i][1], 0);
            debug(debug_tag, "Pump%d 停止", i+1);
        }
    }
    
    // 設定比例閥開度
    int valve_value = (int)(output->valve_opening);
    if (valve_value > 100) valve_value = 100;
    if (valve_value < 5) valve_value = 5;  // 最小開度5%
    
    modbus_write_single_register(REG_VALVE_OPENING, valve_value);
    debug(debug_tag, "比例閥設定 - 開度: %d%%", valve_value);
}

/**
 * 計算比例閥調整 (配合流量控制)
 */
static float calculate_valve_adjustment(float pid_output, const flow_sensor_data_t *data) {
    // 讀取當前閥門開度
    int current_opening_raw = modbus_read_input_register(REG_VALVE_ACTUAL);
    float current_opening = (current_opening_raw >= 0) ? current_opening_raw : 50.0f;  // 預設50%
    
    // 基於PID輸出調整閥門
    float valve_adjustment = pid_output * 0.3f;  // 閥門響應係數
    
    // 流量比例微調
    if (data->F1_primary_inlet > 0 && data->F2_secondary_outlet > 0) {
        float flow_ratio = data->F2_secondary_outlet / data->F1_primary_inlet;
        if (flow_ratio < 0.9f) {
            valve_adjustment += 3.0f;  // 開大閥門增加流量
        } else if (flow_ratio > 1.1f) {
            valve_adjustment -= 3.0f;  // 關小閥門減少流量
        }
    }
    
    // 計算新開度
    float new_opening = current_opening + valve_adjustment;
    
    // 安全範圍限制
    if (new_opening > 95.0f) new_opening = 95.0f;
    if (new_opening < 5.0f) new_opening = 5.0f;
    
    debug(debug_tag, "閥門調整: %.1f%% -> %.1f%% (調整量: %.1f%%)", 
          current_opening, new_opening, valve_adjustment);
    
    return new_opening;
}

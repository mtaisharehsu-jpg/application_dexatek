/*
 * control_logic_ls80_3.c - LS80 流量控制邏輯 (Control Logic 3: Flow Control)
 *
 * 【功能概述】
 * 本模組實現 CDU 系統的流量控制功能,通過 PID 演算法維持冷卻水系統流量穩定,
 * 支援 F2→Fset 追蹤模式,並提供2泵協調控制策略,確保流量精確跟隨設定值。
 *
 * 【控制目標】
 * - 維持二次側出水流量 F2 追蹤設定值 Fset
 * - 預設目標流量: 200.0 L/min
 * - 追蹤模式: F2→Fset (F2 追蹤流量設定值)
 *
 * 【感測器配置】
 * - F1 (REG 42062): 一次側進水流量 (參考, 0.1 L/min 精度)
 * - F2 (REG 42063): 二次側出水流量 (主要控制目標, 0.1 L/min 精度)
 * - F3/F4: 預留用於未來擴展
 *
 * 【執行器控制】
 * - Pump1/2: 泵浦速度 0-100% (REG 45015/45016)
 * - Pump1/2啟停: DO 控制 (REG 411101/411103)
 * - 比例閥: 開度 5-100% (REG 411151, 可選協調)
 *
 * 【控制模式】
 * - 手動模式: 僅監控,操作員手動調整 (REG_PUMP1_MANUAL_MODE)
 * - 自動模式: PID 控制 + 2泵協調策略
 *
 * 【2泵協調策略】
 * 根據 PID 輸出自動決定泵浦運行策略:
 * - PID ≤ 50%: 單泵模式 (Pump1 單獨運行, 最低10%轉速)
 * - 50% < PID ≤ 80%: 雙泵協調模式 (Pump1主控, Pump2輔助, 最低10%轉速)
 * - PID > 80%: 雙泵高速模式 (雙泵協同工作, 最低10%轉速)
 * - PID < 死區(5%): 停止所有泵浦
 *
 * 【PID 參數】
 * - Kp: 2.5 (比例增益 - 流量響應)
 * - Ki: 0.4 (積分增益 - 消除流量偏差)
 * - Kd: 0.8 (微分增益 - 流量變化率抑制)
 * - 輸出範圍: -100% ~ +100%
 * - 積分限幅: 防止飽和
 *
 * 【自適應調整】
 * - 大誤差 (>15%): 增加 Kp, 減少 Ki → 快速響應
 * - 小誤差 (<3%): 減少 Kp, 增加 Ki → 提高穩態精度
 * - 微分項根據誤差變化率動態調整
 *
 * 【安全保護】
 * - 最大流量變化率: 100 L/min/s
 * - 最小控制流量: 30 L/min
 * - 最大追蹤誤差: 50 L/min
 * - 流量上限: REG_FLOW_HIGH_LIMIT
 * - 流量下限: REG_FLOW_LOW_LIMIT
 * - F1/F2 比例一致性檢查: 0.3-1.5
 *
 * 作者: [DK]
 * 日期: 2025
 */

#include "dexatek/main_application/include/application_common.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
static const char *debug_tag = "ls80_3_flow";

#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_ls80_3.json"
#define CONFIG_REGISTER_LIST_SIZE 25
static control_logic_register_t _control_logic_register_list[CONFIG_REGISTER_LIST_SIZE];

// 系統狀態寄存器
static uint32_t REG_CONTROL_LOGIC_2_ENABLE = 41002; // 控制邏輯2啟用
static uint32_t REG_CONTROL_LOGIC_3_ENABLE = 41003; // 控制邏輯3啟用

static uint32_t REG_AUTO_START_STOP = 45020;        // 自動啟停開關

static uint32_t REG_F1_FLOW = 42062;   // F1一次側進水流量
static uint32_t REG_F2_FLOW = 42063;   // F2二次側出水流量 (主要控制)

static uint32_t REG_TARGET_FLOW = 45003;   // 目標流量設定 (F_set)
static uint32_t REG_FLOW_MODE = 45005;   // 流量/壓差模式選擇 (0=流量模式)
static uint32_t REG_FLOW_HIGH_LIMIT = 45006;   // 流量上限
static uint32_t REG_FLOW_LOW_LIMIT = 45007;   // 流量下限

static uint32_t REG_PUMP1_SPEED = 45015;  // Pump1速度設定 (0-1000)
static uint32_t REG_PUMP2_SPEED = 45016;  // Pump2速度設定

static uint32_t REG_PUMP1_CONTROL = 411101;  // Pump1啟停控制
static uint32_t REG_PUMP2_CONTROL = 411102;  // Pump2啟停控制

static uint32_t REG_PUMP1_MANUAL_MODE = 45021;   // Pump1手動模式 (0=自動, 1=手動)
static uint32_t REG_PUMP2_MANUAL_MODE = 45022;   // Pump2手動模式

static uint32_t REG_VALVE_OPENING = 411151;      // [DISABLED] 比例閥開度設定 (%)

static uint32_t REG_VALVE_MANUAL_MODE = 45061;   // [DISABLED] 比例閥手動模式

// 安全限制參數
#define MAX_FLOW_CHANGE_RATE      100.0f  // 最大流量變化率 L/min/sec
#define MIN_CONTROL_FLOW          30.0f   // 最小控制流量
#define MAX_TRACKING_ERROR        50.0f   // 最大追蹤誤差
#define PUMP_MIN_SPEED            10.0f   // 泵浦最小速度 %
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
    int active_pumps[2];         // 泵浦啟用狀態 [Pump1, Pump2]
    float pump_speeds[2];        // 泵浦速度 0-100%
    float valve_opening;         // [DISABLED] 比例閥開度 0-100%
} flow_control_output_t;

// 全域變數
static flow_pid_controller_t flow_pid = {
    .kp = 2.5f,                  // 流量控制比例增益kp:2.5
    .ki = 0.4f,                  // 流量控制積分增益(0.4)
    .kd = 0.8f,                  // 流量控制微分增益(0.8)
    .integral = 0.0f,
    .previous_error = 0.0f,
    .previous_time = 0,
    .output_min = 0.0f,
    .output_max = 100.0f
};

static flow_control_config_t flow_config = {
    .tracking_mode = FLOW_TRACKING_MODE_F2_TO_FSET,
    .target_flow_rate = 200.0f,  // 預設目標流量 200 L/min
    .flow_high_limit = 50.0f,   // 流量上限 500 L/min
    .flow_low_limit = 10.0f,     // 流量下限 50 L/min
    .tracking_ratio = 1.0f       // 1:1追蹤比例
};

// 追蹤 enable 狀態，用於檢測從啟用變為停用
static uint16_t previous_enable_status = 0;

// 追蹤 AUTO_START_STOP 狀態，用於邊緣觸發檢測
static uint16_t previous_auto_start_stop = 0;

// 追蹤 FLOW_MODE 狀態，用於邊緣觸發檢測
static uint16_t previous_flow_mode = 0;

// 追蹤 PUMP_MANUAL_MODE 狀態，用於 FLOW_MODE 切換時保持不變
static uint16_t saved_pump1_manual_mode = 0xFFFF;  // 0xFFFF 表示未初始化
static uint16_t saved_pump2_manual_mode = 0xFFFF;

/*---------------------------------------------------------------------------
							Function Prototypes
 ---------------------------------------------------------------------------*/
// 函數宣告
static int read_flow_sensor_data(flow_sensor_data_t *data);
static flow_safety_status_t perform_flow_safety_checks(const flow_sensor_data_t *data, float target_flow);
static void emergency_flow_shutdown(void);
static void handle_auto_start_stop(void);
static void restore_pump_manual_mode_if_saved(void);
static float calculate_flow_tracking_target(const flow_sensor_data_t *data);
static float calculate_flow_pid_output(flow_pid_controller_t *pid, float setpoint, float current_value);
static void reset_flow_pid_controller(flow_pid_controller_t *pid);
static void adaptive_flow_pid_tuning(flow_pid_controller_t *pid, float error, float error_percentage);
static int execute_manual_flow_control_mode(float target_flow);
static int execute_automatic_flow_control_mode(const flow_sensor_data_t *data);
static void calculate_basic_pump_control(float pid_output, flow_control_output_t *output);
static void execute_flow_control_output(const flow_control_output_t *output);
//static float calculate_valve_adjustment(float pid_output, const flow_sensor_data_t *data);

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

/**
 * 切換到手動模式並保存最後速度值
 *
 * 當 control_logic_3 從啟用變為停用時調用
 */
static void switch_to_manual_mode_with_last_speed(void) {
    info(debug_tag, "control_logic_3 停用，切換到手動模式...");

    // 1. 讀取當前泵浦速度值（最後的速度）
    uint16_t pump1_speed = modbus_read_input_register(REG_PUMP1_SPEED);
    uint16_t pump2_speed = modbus_read_input_register(REG_PUMP2_SPEED);

    info(debug_tag, "保存最後速度值 - Pump1: %d, Pump2: %d",
         pump1_speed, pump2_speed);

    // 2. 重新寫入速度值（確保保持當前速度）
    modbus_write_single_register(REG_PUMP1_SPEED, pump1_speed);
    modbus_write_single_register(REG_PUMP2_SPEED, pump2_speed);

    // 3. 設定為手動模式
    modbus_write_single_register(REG_PUMP1_MANUAL_MODE, 1);
    modbus_write_single_register(REG_PUMP2_MANUAL_MODE, 1);

    info(debug_tag, "已切換到手動模式 - Pump1=%d, Pump2=%d (手動模式=1)",
         pump1_speed, pump2_speed);
}

/**
 * 處理 AUTO_START_STOP 與 FLOW_MODE 寄存器的聯動控制
 *
 * 【舊邏輯 - 保留】AUTO_START_STOP 0→1 且 FLOW_MODE=0
 * - 動作：ENABLE_3=1
 *
 * 【需求2】AUTO_START_STOP=0 時持續檢查
 * - 動作：持續強制 ENABLE_3=0, ENABLE_2=0
 *
 * 【需求1A】FLOW_MODE 0→1 (在 AUTO_START_STOP=1 時)
 * - 動作：ENABLE_3=0, ENABLE_2=1
 *
 * 【需求1B】FLOW_MODE 1→0 (在 AUTO_START_STOP=1 時)
 * - 動作：ENABLE_3=1, ENABLE_2=0
 */
static void handle_auto_start_stop(void) {
    // 讀取當前寄存器狀態
    uint16_t current_auto_start_stop = modbus_read_input_register(REG_AUTO_START_STOP);
    uint16_t current_flow_mode = modbus_read_input_register(REG_FLOW_MODE);

    // 檢查讀取是否成功
    if (current_auto_start_stop == 0xFFFF || current_flow_mode == 0xFFFF) {
        warn(debug_tag, "寄存器讀取失敗，跳過聯動控制");
        return;
    }

    // 【需求2 - 最高優先級】AUTO_START_STOP=0 時，持續強制 ENABLE_2=0, ENABLE_3=0
    if (current_auto_start_stop == 0) {
        modbus_write_single_register(REG_CONTROL_LOGIC_2_ENABLE, 0);
        modbus_write_single_register(REG_CONTROL_LOGIC_3_ENABLE, 0);

        // 更新狀態並返回
        previous_auto_start_stop = current_auto_start_stop;
        previous_flow_mode = current_flow_mode;
        return;
    }

    // 以下邏輯只在 AUTO_START_STOP=1 時執行

    // 【舊邏輯 - 保留】AUTO_START_STOP 0→1 且 FLOW_MODE=0 → ENABLE_3=1
    if (previous_auto_start_stop == 0 && current_auto_start_stop == 1) {
        if (current_flow_mode == 0) {
            bool success = modbus_write_single_register(REG_CONTROL_LOGIC_3_ENABLE, 1);
            if (success) {
                info(debug_tag, "【舊邏輯】AUTO_START_STOP 0→1 且 FLOW_MODE=0 → ENABLE_3=1");
            }
        }
        // 注意: FLOW_MODE=1 的情況由 ls80_2.c 處理 (需求3A)
    }

    // 【需求1A】FLOW_MODE 0→1 邊緣觸發 (只在 AUTO_START_STOP=1 時)
    if (previous_flow_mode == 0 && current_flow_mode == 1) {
        // 保存當前 PUMP_MANUAL_MODE 狀態
        saved_pump1_manual_mode = modbus_read_input_register(REG_PUMP1_MANUAL_MODE);
        saved_pump2_manual_mode = modbus_read_input_register(REG_PUMP2_MANUAL_MODE);
        info(debug_tag, "【FLOW_MODE切換】保存 PUMP_MANUAL_MODE - P1=%d, P2=%d",
             saved_pump1_manual_mode, saved_pump2_manual_mode);

        // FLOW_MODE 從流量模式切換到壓差模式
        bool success1 = modbus_write_single_register(REG_CONTROL_LOGIC_3_ENABLE, 0);
        bool success2 = modbus_write_single_register(REG_CONTROL_LOGIC_2_ENABLE, 1);

        if (success1 && success2) {
            info(debug_tag, "【需求1A】FLOW_MODE 0→1 (AUTO_START_STOP=1) → ENABLE_3=0, ENABLE_2=1");
        }
    }

    // 【需求1B】FLOW_MODE 1→0 邊緣觸發 (只在 AUTO_START_STOP=1 時)
    if (previous_flow_mode == 1 && current_flow_mode == 0) {
        // 保存當前 PUMP_MANUAL_MODE 狀態
        saved_pump1_manual_mode = modbus_read_input_register(REG_PUMP1_MANUAL_MODE);
        saved_pump2_manual_mode = modbus_read_input_register(REG_PUMP2_MANUAL_MODE);
        info(debug_tag, "【FLOW_MODE切換】保存 PUMP_MANUAL_MODE - P1=%d, P2=%d",
             saved_pump1_manual_mode, saved_pump2_manual_mode);

        // FLOW_MODE 從壓差模式切換回流量模式
        bool success1 = modbus_write_single_register(REG_CONTROL_LOGIC_3_ENABLE, 1);
        bool success2 = modbus_write_single_register(REG_CONTROL_LOGIC_2_ENABLE, 0);

        if (success1 && success2) {
            info(debug_tag, "【需求1B】FLOW_MODE 1→0 (AUTO_START_STOP=1) → ENABLE_3=1, ENABLE_2=0");
        }
    }

    // 更新前次狀態
    previous_auto_start_stop = current_auto_start_stop;
    previous_flow_mode = current_flow_mode;
}

/**
 * 恢復 PUMP_MANUAL_MODE 狀態 (如果在 FLOW_MODE 切換時有保存)
 *
 * 【功能說明】
 * 當 FLOW_MODE 切換時,會保存 PUMP_MANUAL_MODE 的原始狀態。
 * 在控制邏輯執行完畢後,調用此函數恢復原始狀態,確保:
 * - PUMP1_MANUAL_MODE_REG 和 PUMP2_MANUAL_MODE_REG 保持 FLOW_MODE 切換前的值
 * - 不受 switch_to_manual_mode_with_last_speed() 或 execute_automatic_xxx_control() 影響
 *
 * 【使用時機】
 * 在 control_logic_ls80_3_flow_control() 主函數的最後調用
 */
static void restore_pump_manual_mode_if_saved(void) {
    if (saved_pump1_manual_mode != 0xFFFF) {
        modbus_write_single_register(REG_PUMP1_MANUAL_MODE, saved_pump1_manual_mode);
        modbus_write_single_register(REG_PUMP2_MANUAL_MODE, saved_pump2_manual_mode);

        info(debug_tag, "【FLOW_MODE切換】恢復 PUMP_MANUAL_MODE - P1=%d, P2=%d",
             saved_pump1_manual_mode, saved_pump2_manual_mode);

        // 清除保存的值
        saved_pump1_manual_mode = 0xFFFF;
        saved_pump2_manual_mode = 0xFFFF;
    }
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

    _control_logic_register_list[3].name = REG_FLOW_SETPOINT_STR;
    _control_logic_register_list[3].address_ptr = &REG_TARGET_FLOW;
    _control_logic_register_list[3].default_address = REG_TARGET_FLOW;
    _control_logic_register_list[3].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[4].name = REG_FLOW_MODE_STR;
    _control_logic_register_list[4].address_ptr = &REG_FLOW_MODE;
    _control_logic_register_list[4].default_address = REG_FLOW_MODE;
    _control_logic_register_list[4].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[5].name = REG_FLOW_HIGH_LIMIT_STR;
    _control_logic_register_list[5].address_ptr = &REG_FLOW_HIGH_LIMIT;
    _control_logic_register_list[5].default_address = REG_FLOW_HIGH_LIMIT;
    _control_logic_register_list[5].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[6].name = REG_FLOW_LOW_LIMIT_STR;
    _control_logic_register_list[6].address_ptr = &REG_FLOW_LOW_LIMIT;
    _control_logic_register_list[6].default_address = REG_FLOW_LOW_LIMIT;
    _control_logic_register_list[6].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[7].name = REG_PUMP1_SPEED_STR;
    _control_logic_register_list[7].address_ptr = &REG_PUMP1_SPEED;
    _control_logic_register_list[7].default_address = REG_PUMP1_SPEED;
    _control_logic_register_list[7].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[8].name = REG_PUMP2_SPEED_STR;
    _control_logic_register_list[8].address_ptr = &REG_PUMP2_SPEED;
    _control_logic_register_list[8].default_address = REG_PUMP2_SPEED;
    _control_logic_register_list[8].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[9].name = REG_PUMP1_CONTROL_STR;
    _control_logic_register_list[9].address_ptr = &REG_PUMP1_CONTROL;
    _control_logic_register_list[9].default_address = REG_PUMP1_CONTROL;
    _control_logic_register_list[9].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[10].name = REG_PUMP2_CONTROL_STR;
    _control_logic_register_list[10].address_ptr = &REG_PUMP2_CONTROL;
    _control_logic_register_list[10].default_address = REG_PUMP2_CONTROL;
    _control_logic_register_list[10].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[11].name = REG_PUMP1_MANUAL_MODE_STR;
    _control_logic_register_list[11].address_ptr = &REG_PUMP1_MANUAL_MODE;
    _control_logic_register_list[11].default_address = REG_PUMP1_MANUAL_MODE;
    _control_logic_register_list[11].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[12].name = REG_PUMP2_MANUAL_MODE_STR;
    _control_logic_register_list[12].address_ptr = &REG_PUMP2_MANUAL_MODE;
    _control_logic_register_list[12].default_address = REG_PUMP2_MANUAL_MODE;
    _control_logic_register_list[12].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    /*
    _control_logic_register_list[13].name = REG_VALVE_SETPOINT_STR;
    _control_logic_register_list[13].address_ptr = &REG_VALVE_OPENING;
    _control_logic_register_list[13].default_address = REG_VALVE_OPENING;
    _control_logic_register_list[13].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[14].name = REG_VALVE_MANUAL_MODE_STR;
    _control_logic_register_list[14].address_ptr = &REG_VALVE_MANUAL_MODE;
    _control_logic_register_list[14].default_address = REG_VALVE_MANUAL_MODE;
    _control_logic_register_list[14].type = CONTROL_LOGIC_REGISTER_READ_WRITE;
    */

    uint32_t list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    ret = control_logic_register_load_from_file(CONFIG_REGISTER_FILE_PATH, _control_logic_register_list, list_size);
    debug(debug_tag, "load register array from file %s, ret %d", CONFIG_REGISTER_FILE_PATH, ret);

    return ret;
}

int control_logic_ls80_3_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

int control_logic_ls80_3_flow_control_init(void) 
{
    int ret = SUCCESS;

    _register_list_init();

    return ret;
}


/**
 * CDU流量控制主要函數 (版本 3.1)
 *
 * 【函數功能】
 * 這是流量控制邏輯的主入口函數,由控制邏輯管理器週期性調用。
 * 實現 F2→Fset 流量追蹤控制: 啟用檢查 → 感測器讀取 → 安全檢查 → 模式判斷 → 控制執行
 *
 * 【執行流程】
 * 1. 檢查控制邏輯是否啟用 (REG_CONTROL_LOGIC_3_ENABLE)
 * 2. 讀取流量感測器數據 (F1, F2, F3, F4)
 * 3. 計算追蹤目標流量 (F2→Fset 模式)
 * 4. 執行安全檢查 (流量上下限/追蹤誤差/感測器一致性)
 * 5. 根據模式執行對應的控制邏輯:
 *    - 手動模式: 僅監控,操作員通過 HMI 控制
 *    - 自動模式: PID 控制 + 自適應參數 + 雙泵協調
 *
 * @param ptr 控制邏輯結構指標 (本函數未使用)
 * @return 0=成功, -1=感測器讀取失敗, -2=緊急停機, 其他=控制執行失敗
 */
int control_logic_ls80_3_flow_control(ControlLogic *ptr) {

    (void)ptr;

    // 處理 AUTO_START_STOP 邊緣觸發
    handle_auto_start_stop();

    // 【步驟0】檢測 enable 從 1 變為 0，觸發切換到手動模式
    uint16_t current_enable = modbus_read_input_register(REG_CONTROL_LOGIC_3_ENABLE);

    if (previous_enable_status == 1 && current_enable == 0) {
        // enable 從啟用變為停用，執行切換到手動模式
        switch_to_manual_mode_with_last_speed();
    }

    // 更新前次狀態
    previous_enable_status = current_enable;

    // 【步驟1】檢查控制邏輯3是否啟用 (通過 Modbus 寄存器 41003)
    if (current_enable != 1) {
        return 0;  // 未啟用則直接返回,不執行控制
    }

    flow_sensor_data_t sensor_data;
    flow_safety_status_t safety_status;
    int control_mode;
    int ret = 0;

    info(debug_tag, "=== CDU流量控制系統執行 (v3.1) ===");

    // 【步驟2】讀取流量感測器數據
    // 包括 F1(一次側進水), F2(二次側出水,主要控制目標), F3/F4(預留)
    if (read_flow_sensor_data(&sensor_data) != 0) {
        error(debug_tag, "讀取流量感測器數據失敗");
        return -1;
    }

    debug(debug_tag, "流量數據 - F1: %.1f, F2: %.1f, F3: %.1f, F4: %.1f L/min",
          sensor_data.F1_primary_inlet, sensor_data.F2_secondary_outlet,
          sensor_data.F3_secondary_inlet, sensor_data.F4_primary_outlet);

    // 【步驟3】計算追蹤目標流量 (F2→Fset: F2 追蹤流量設定值)
    float target_flow = calculate_flow_tracking_target(&sensor_data);

    // 【步驟4】安全檢查
    // 檢查項目: 流量上下限/追蹤誤差/F1與F2比例一致性
    safety_status = perform_flow_safety_checks(&sensor_data, target_flow);

    if (safety_status == FLOW_SAFETY_STATUS_EMERGENCY) {
        // 緊急等級 → 立即停機
        error(debug_tag, "流量控制緊急狀況，執行緊急停機");
        emergency_flow_shutdown();
        return -2;
    } else if (safety_status == FLOW_SAFETY_STATUS_CRITICAL) {
        // 嚴重等級 → 採取保護措施
        warn(debug_tag, "流量控制嚴重警告狀態");
    } else if (safety_status == FLOW_SAFETY_STATUS_WARNING) {
        // 警告等級 → 繼續監控
        warn(debug_tag, "流量控制警告狀態，繼續監控");
    }

    // 【步驟5】檢查控制模式 (基於手動模式寄存器)
    // 任何一個設備在手動模式,整個系統就是手動模式
    int pump1_manual = modbus_read_input_register(REG_PUMP1_MANUAL_MODE);
    // int valve_manual = modbus_read_input_register(REG_VALVE_MANUAL_MODE);  // [DISABLED] 比例閥不使用

    control_mode = (pump1_manual > 0 /* || valve_manual > 0 */) ? FLOW_CONTROL_MODE_MANUAL : FLOW_CONTROL_MODE_AUTO;

    // 【步驟6】根據控制模式執行相應邏輯
    if (control_mode == FLOW_CONTROL_MODE_AUTO) {
        // 自動模式: PID 控制 + 自適應參數調整 + 雙泵協調
        info(debug_tag, "執行自動流量控制模式 (F2→Fset追蹤)");
        ret = execute_automatic_flow_control_mode(&sensor_data);
    } else {
        // 手動模式: 僅監控狀態,由操作員手動控制
        info(debug_tag, "手動流量控制模式 - 僅監控狀態");
        ret = execute_manual_flow_control_mode(target_flow);
    }

    if (ret != 0) {
        error(debug_tag, "流量控制邏輯執行失敗: %d", ret);
    }

    // 恢復 PUMP_MANUAL_MODE 狀態 (如果在 FLOW_MODE 切換時有保存)
    restore_pump_manual_mode_if_saved();

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
    // flow_raw = modbus_read_input_register(REG_F3_FLOW);
    // if (flow_raw >= 0) {
    //     data->F3_secondary_inlet = flow_raw / 10.0f;
    // } else {
    //     warn(debug_tag, "F3流量讀取失敗");
    //     data->F3_secondary_inlet = 0.0f;
    // }
    
    // // 讀取F4一次側出水流量
    // flow_raw = modbus_read_input_register(REG_F4_FLOW);
    // if (flow_raw >= 0) {
    //     data->F4_primary_outlet = flow_raw / 10.0f;
    // } else {
    //     warn(debug_tag, "F4流量讀取失敗");
    //     data->F4_primary_outlet = 0.0f;
    // }
    
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
static flow_safety_status_t perform_flow_safety_checks(const flow_sensor_data_t *data, float target_flow) 
{
    (void)target_flow;
    flow_safety_status_t status = FLOW_SAFETY_STATUS_SAFE;
    
    // // 1. 緊急停機檢查
    // if (target_flow > flow_config.flow_high_limit) {
    //     error(debug_tag, "目標流量超出安全上限: %.1f > %.1f L/min", 
    //           target_flow, flow_config.flow_high_limit);
    //     return FLOW_SAFETY_STATUS_EMERGENCY;
    // }
    
    // if (data->F2_secondary_outlet < MIN_CONTROL_FLOW) {
    //     error(debug_tag, "F2流量過低: %.1f < %.1f L/min", 
    //           data->F2_secondary_outlet, MIN_CONTROL_FLOW);
    //     return FLOW_SAFETY_STATUS_EMERGENCY;
    // }
    
    // // 2. 嚴重警告檢查
    // float tracking_error = fabs(data->F2_secondary_outlet - target_flow);
    // if (tracking_error > MAX_TRACKING_ERROR) {
    //     warn(debug_tag, "F2流量追蹤誤差過大: %.1f L/min", tracking_error);
    //     status = FLOW_SAFETY_STATUS_CRITICAL;
    // }
    
    // // 3. 警告條件檢查
    // if (data->F2_secondary_outlet > flow_config.flow_high_limit * 0.9f) {
    //     warn(debug_tag, "F2流量接近上限: %.1f L/min", data->F2_secondary_outlet);
    //     status = (status == FLOW_SAFETY_STATUS_SAFE) ? FLOW_SAFETY_STATUS_WARNING : status;
    // }
    
    // if (data->F2_secondary_outlet < flow_config.flow_low_limit) {
    //     warn(debug_tag, "F2流量低於下限: %.1f L/min", data->F2_secondary_outlet);
    //     status = (status == FLOW_SAFETY_STATUS_SAFE) ? FLOW_SAFETY_STATUS_WARNING : status;
    // }
    
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
    // modbus_write_single_register(REG_PUMP3_CONTROL, 0);
    
    // 比例閥設置到安全開度 (30%)
    // modbus_write_single_register(REG_VALVE_OPENING, 30);
    
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
    
    debug(debug_tag, "流量PID012 - 誤差: %.2f, P: %.2f, I: %.2f, D: %.2f, 輸出: %.2f", 
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
    //modbus_write_single_register(REG_PUMP1_MANUAL_MODE, 1);
    // modbus_write_single_register(REG_VALVE_MANUAL_MODE, 1);
    
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
    // modbus_write_single_register(REG_PUMP3_MANUAL_MODE, 0);
    // modbus_write_single_register(REG_VALVE_MANUAL_MODE, 0);
    
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
    // control_output.valve_opening = calculate_valve_adjustment(pid_output, data);
    
    // 執行控制輸出
    execute_flow_control_output(&control_output);
    
    info(debug_tag, "自動流量控制完成 - PID輸出: %.1f%%, 泵浦速度: Pump1=%.1f%%, Pump2=%.1f%%",
         pid_output, control_output.pump_speeds[0], control_output.pump_speeds[1]);
    
    return 0;
}

/**
 * 計算基礎泵浦控制策略 (使用PID精確控制泵速)
 */
static void calculate_basic_pump_control(float pid_output, flow_control_output_t *output) {
    // PID輸出範圍應為 [-100, +100]，映射到泵速控制
    float abs_pid_output = fabs(pid_output);
    
    // 死區處理：小於5%的輸出視為無需調整
    const float CONTROL_DEADZONE = 5.0f;
    
    // 初始化輸出
    for (int i = 0; i < 2; i++) {
        output->active_pumps[i] = 0;
        output->pump_speeds[i] = 0.0f;
    }
    
    if (abs_pid_output < CONTROL_DEADZONE) {
        // 在死區內，停止所有泵浦或維持最小運行
        debug(debug_tag, "PID輸出在死區內(%.1f%%)，停止泵浦", pid_output);
        return;
    }
    
    // 將PID輸出映射到實際需要的泵速
    float base_speed = abs_pid_output;
    
    // 確保速度在安全範圍內
    if (base_speed > PUMP_MAX_SPEED) {
        base_speed = PUMP_MAX_SPEED;
    } else if (base_speed < PUMP_MIN_SPEED) {
        base_speed = PUMP_MIN_SPEED;
    }
    
    // 雙泵協調控制策略
    if (base_speed <= 50.0f) {
        // 小到中等流量需求：僅使用Pump1
        output->active_pumps[0] = 1;
        output->pump_speeds[0] = base_speed;
        output->active_pumps[1] = 0;
        output->pump_speeds[1] = 0.0f;
        
        debug(debug_tag, "單泵模式 - Pump1: %.1f%%", base_speed);
        
    } else if (base_speed <= 80.0f) {
        // 中高流量需求：Pump1主控，Pump2輔助
        output->active_pumps[0] = 1;
        output->pump_speeds[0] = base_speed;
        output->active_pumps[1] = 1;
        output->pump_speeds[1] = (base_speed - 50.0f) * 0.6f; // Pump2較保守
        
        debug(debug_tag, "雙泵協調模式 - Pump1: %.1f%%, Pump2: %.1f%%", 
              output->pump_speeds[0], output->pump_speeds[1]);
              
    } else {
        // 高流量需求：雙泵協同工作
        output->active_pumps[0] = 1;
        output->pump_speeds[0] = base_speed;
        output->active_pumps[1] = 1;
        output->pump_speeds[1] = base_speed * 0.8f; // Pump2跟隨主泵
        
        debug(debug_tag, "雙泵高速模式 - Pump1: %.1f%%, Pump2: %.1f%%", 
              output->pump_speeds[0], output->pump_speeds[1]);
    }
    
    // 確保所有運行中的泵速度不低於最小值 (10%)
    if (output->active_pumps[0] && output->pump_speeds[0] < PUMP_MIN_SPEED) {
        output->pump_speeds[0] = PUMP_MIN_SPEED;
    }
    if (output->active_pumps[1] && output->pump_speeds[1] < PUMP_MIN_SPEED) {
        output->pump_speeds[1] = PUMP_MIN_SPEED;
    }

    debug(debug_tag, "泵浦控制計算完成 - PID: %.1f%%, 輸出速度: Pump1=%.1f%%, Pump2=%.1f%%",
          pid_output, output->pump_speeds[0], output->pump_speeds[1]);
}

/**
 * 執行流量控制輸出
 */
static void execute_flow_control_output(const flow_control_output_t *output) {
    int pump_registers[2][2] = {
        {REG_PUMP1_SPEED, REG_PUMP1_CONTROL},
        {REG_PUMP2_SPEED, REG_PUMP2_CONTROL}
    };

    // 控制2個泵浦
    for (int i = 0; i < 2; i++) {
        if (output->active_pumps[i]) {
            // 啟動並設定速度 (0-1000對應0-100%)
            // int speed_value = (int)(output->pump_speeds[i] * 10.0f);
            // if (speed_value > 1000) speed_value = 1000;
            // if (speed_value < 0) speed_value = 0;

            int speed_value = (int)(output->pump_speeds[i]);
            if (speed_value > 100) speed_value = 100;
            if (speed_value < 0) speed_value = 0;

            modbus_write_single_register(pump_registers[i][0], speed_value);
            modbus_write_single_register(pump_registers[i][1], 1);
            
            debug(debug_tag, "Pump%d 啟動 - 速度: %d (%.1f%%)", i+1, speed_value, output->pump_speeds[i]);
        } else {
            // 停止泵浦
            modbus_write_single_register(pump_registers[i][0], 0);
            modbus_write_single_register(pump_registers[i][1], 0);
            debug(debug_tag, "Pump%d 停止", i+1);
        }
    }

    /*
    // 設定比例閥開度
    int valve_value = (int)(output->valve_opening);
    if (valve_value > 100) valve_value = 100;
    if (valve_value < 5) valve_value = 5;  // 最小開度5%

    modbus_write_single_register(REG_VALVE_OPENING, valve_value);
    debug(debug_tag, "比例閥設定 - 開度: %d%%", valve_value);
    */
}

// /**
//  * 計算比例閥調整 (配合流量控制)
//  */
// static float calculate_valve_adjustment(float pid_output, const flow_sensor_data_t *data) {
//     // 讀取當前閥門開度
//     // TBD int current_opening_raw = modbus_read_input_register(REG_VALVE_ACTUAL);
//     // TBD float current_opening = (current_opening_raw >= 0) ? current_opening_raw : 50.0f;  // 預設50%
//     float current_opening = 50.0f;
    
//     // 基於PID輸出調整閥門
//     float valve_adjustment = pid_output * 0.3f;  // 閥門響應係數
    
//     // 流量比例微調
//     if (data->F1_primary_inlet > 0 && data->F2_secondary_outlet > 0) {
//         float flow_ratio = data->F2_secondary_outlet / data->F1_primary_inlet;
//         if (flow_ratio < 0.9f) {
//             valve_adjustment += 3.0f;  // 開大閥門增加流量
//         } else if (flow_ratio > 1.1f) {
//             valve_adjustment -= 3.0f;  // 關小閥門減少流量
//         }
//     }
    
//     // 計算新開度
//     float new_opening = current_opening + valve_adjustment;
    
//     // 安全範圍限制
//     if (new_opening > 95.0f) new_opening = 95.0f;
//     if (new_opening < 5.0f) new_opening = 5.0f;
    
//     debug(debug_tag, "閥門調整: %.1f%% -> %.1f%% (調整量: %.1f%%)", 
//           current_opening, new_opening, valve_adjustment);
    
//     return new_opening;
// }

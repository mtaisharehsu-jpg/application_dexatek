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
 * 
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

// 顯示時間持久化配置
#define DISPLAY_TIME_PERSIST_FILE "/usrdata/ls80_3_display_time.json"
#define DISPLAY_TIME_SAVE_INTERVAL 300  // 每 5 分鐘保存一次 (秒)
static time_t last_display_time_save = 0;  // 上次保存時間戳

// 系統狀態寄存器
static uint32_t REG_CONTROL_LOGIC_2_ENABLE = 41002; // 控制邏輯2啟用
static uint32_t REG_CONTROL_LOGIC_3_ENABLE = 41003; // 控制邏輯3啟用

static uint32_t REG_AUTO_START_STOP = 45020;        // 自動啟停開關

static uint32_t REG_F1_FLOW = 42062;   // F1一次側進水流量
static uint32_t REG_F2_FLOW = 42063;   // F2二次側出水流量 (主要控制)

static uint32_t REG_TARGET_FLOW = 45003;   // 目標流量設定 (F_set)
static uint32_t REG_FLOW_MODE = 45005;   // 流量/壓差模式選擇 (0=流量模式)
static uint32_t REG_FLOW_HIGH_LIMIT = 46401;   // 流量上限 (HMI 可設定,斷電保持)
static uint32_t REG_FLOW_LOW_LIMIT = 46402;   // 流量下限 (HMI 可設定,斷電保持)

static uint32_t REG_PUMP1_SPEED = 45015;  // Pump1速度設定 (0-1000)
static uint32_t REG_PUMP2_SPEED = 45016;  // Pump2速度設定

static uint32_t REG_PUMP1_CONTROL = 411101;  // Pump1啟停控制
static uint32_t REG_PUMP2_CONTROL = 411102;  // Pump2啟停控制

static uint32_t REG_PUMP1_MANUAL_MODE = 45021;   // Pump1手動模式 (0=自動, 1=手動)
static uint32_t REG_PUMP2_MANUAL_MODE = 45022;   // Pump2手動模式

static uint32_t REG_VALVE_OPENING = 411151;      // [DISABLED] 比例閥開度設定 (%)

static uint32_t REG_VALVE_MANUAL_MODE = 45061;   // [DISABLED] 比例閥手動模式

// 主泵輪換相關寄存器
static uint32_t REG_PUMP_SWITCH_HOUR = 45034;      // 主泵切換時數設定 (小時, 0=停用自動切換)
static uint32_t REG_PUMP1_USE = 45036;             // Pump1 啟用開關 (0=停用, 1=啟用) - 避免與 REG_PUMP2_STOP 衝突
static uint32_t REG_PUMP2_USE = 45037;             // Pump2 啟用開關 (0=停用, 1=啟用) - 避免與 REG_PUMP3_STOP 衝突
static uint32_t REG_PRIMARY_PUMP_INDEX = 45045;    // 當前主泵編號 (1=Pump1, 2=Pump2) - HMI 可指定

// 當前主泵 AUTO 模式累積時間顯示寄存器 (獨立累積,用於顯示和切換判斷)
static uint32_t REG_CURRENT_PRIMARY_AUTO_HOURS = 45046;    // 顯示用累積小時
static uint32_t REG_CURRENT_PRIMARY_AUTO_MINUTES = 45047;  // 顯示用累積分鐘

// AUTO 模式累計時間寄存器 (斷電保持)
static uint32_t REG_PUMP1_AUTO_MODE_HOURS = 42170;    // Pump1 作為主泵在 AUTO 模式累計時間 (小時)
static uint32_t REG_PUMP2_AUTO_MODE_HOURS = 42171;    // Pump2 作為主泵在 AUTO 模式累計時間 (小時)
static uint32_t REG_PUMP1_AUTO_MODE_MINUTES = 42172;  // Pump1 AUTO 模式累計時間 (分鐘)
static uint32_t REG_PUMP2_AUTO_MODE_MINUTES = 42173;  // Pump2 AUTO 模式累計時間 (分鐘)

// 安全限制參數
#define MAX_FLOW_CHANGE_RATE      100.0f  // 最大流量變化率 L/min/sec
#define MIN_CONTROL_FLOW          10.0f   // 最小控制流量
#define MAX_TRACKING_ERROR        50.0f   // 最大追蹤誤差
#define PUMP_MIN_SPEED            30.0f   // 泵浦最小運轉速度 30%
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

// 主泵 AUTO 模式時間追蹤結構
typedef struct {
    time_t last_update_time;       // 上次更新時間戳
    bool last_auto_mode_state;     // 上次 AUTO 模式狀態
    bool initialized;              // 是否已初始化
} primary_pump_auto_tracker_t;

// 全局追蹤器
static primary_pump_auto_tracker_t pump1_auto_tracker = {0, false, false};
static primary_pump_auto_tracker_t pump2_auto_tracker = {0, false, false};

// 顯示時間追蹤結構 (獨立於 Pump1/Pump2 的累積)
typedef struct {
    time_t last_update_time;       // 上次更新時間戳
    bool last_auto_mode_state;     // 上次 AUTO 模式狀態
    bool initialized;              // 是否已初始化
    uint16_t accumulated_seconds;  // 累積秒數 (用於進位)
} display_time_tracker_t;

// 顯示時間全局追蹤器
static display_time_tracker_t display_tracker = {0, false, false, 0};

// 追蹤主泵變化 (用於偵測 HMI 手動修改)
static uint16_t last_primary_pump_index = 0;

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
static void update_display_auto_time(void);
static void check_and_switch_primary_pump(void);
static int save_display_time_to_file(void);
static int restore_display_time_from_file(void);
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
   // modbus_write_single_register(REG_PUMP1_MANUAL_MODE, 1);
   // modbus_write_single_register(REG_PUMP2_MANUAL_MODE, 1);

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
        // 檢測 AUTO_START_STOP 1→0 邊緣,保存 PUMP_MANUAL_MODE
        if (previous_auto_start_stop == 1) {
            saved_pump1_manual_mode = modbus_read_input_register(REG_PUMP1_MANUAL_MODE);
            saved_pump2_manual_mode = modbus_read_input_register(REG_PUMP2_MANUAL_MODE);
            info(debug_tag, "【AUTO_START_STOP 1→0】保存 PUMP_MANUAL_MODE - P1=%d, P2=%d",
                 saved_pump1_manual_mode, saved_pump2_manual_mode);

            // 設定泵浦轉速為 0
            modbus_write_single_register(REG_PUMP1_SPEED, 0);
            modbus_write_single_register(REG_PUMP2_SPEED, 0);
            info(debug_tag, "【AUTO_START_STOP 1→0】設定泵浦轉速為 0");
        }

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

    _control_logic_register_list[5].name = REG_F_HIGH_ALARM_STR;
    _control_logic_register_list[5].address_ptr = &REG_FLOW_HIGH_LIMIT;
    _control_logic_register_list[5].default_address = 46401;
    _control_logic_register_list[5].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[6].name = REG_F_LOW_ALARM_STR;
    _control_logic_register_list[6].address_ptr = &REG_FLOW_LOW_LIMIT;
    _control_logic_register_list[6].default_address = 46402;
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

    // 主泵輪換控制寄存器
    _control_logic_register_list[13].name = REG_PUMP_SWITCH_HOUR_STR;
    _control_logic_register_list[13].address_ptr = &REG_PUMP_SWITCH_HOUR;
    _control_logic_register_list[13].default_address = REG_PUMP_SWITCH_HOUR;
    _control_logic_register_list[13].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[14].name = REG_PRIMARY_PUMP_INDEX_STR;
    _control_logic_register_list[14].address_ptr = &REG_PRIMARY_PUMP_INDEX;
    _control_logic_register_list[14].default_address = REG_PRIMARY_PUMP_INDEX;
    _control_logic_register_list[14].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[15].name = REG_PUMP1_USE_STR;
    _control_logic_register_list[15].address_ptr = &REG_PUMP1_USE;
    _control_logic_register_list[15].default_address = REG_PUMP1_USE;
    _control_logic_register_list[15].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[16].name = REG_PUMP2_USE_STR;
    _control_logic_register_list[16].address_ptr = &REG_PUMP2_USE;
    _control_logic_register_list[16].default_address = REG_PUMP2_USE;
    _control_logic_register_list[16].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // AUTO 模式累計時間寄存器
    _control_logic_register_list[17].name = REG_PUMP1_AUTO_MODE_HOURS_STR;
    _control_logic_register_list[17].address_ptr = &REG_PUMP1_AUTO_MODE_HOURS;
    _control_logic_register_list[17].default_address = REG_PUMP1_AUTO_MODE_HOURS;
    _control_logic_register_list[17].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[18].name = REG_PUMP2_AUTO_MODE_HOURS_STR;
    _control_logic_register_list[18].address_ptr = &REG_PUMP2_AUTO_MODE_HOURS;
    _control_logic_register_list[18].default_address = REG_PUMP2_AUTO_MODE_HOURS;
    _control_logic_register_list[18].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[19].name = REG_PUMP1_AUTO_MODE_MINUTES_STR;
    _control_logic_register_list[19].address_ptr = &REG_PUMP1_AUTO_MODE_MINUTES;
    _control_logic_register_list[19].default_address = REG_PUMP1_AUTO_MODE_MINUTES;
    _control_logic_register_list[19].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[20].name = REG_PUMP2_AUTO_MODE_MINUTES_STR;
    _control_logic_register_list[20].address_ptr = &REG_PUMP2_AUTO_MODE_MINUTES;
    _control_logic_register_list[20].default_address = REG_PUMP2_AUTO_MODE_MINUTES;
    _control_logic_register_list[20].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // 當前主泵 AUTO 累積時間顯示寄存器 (獨立累積, 支援斷電保持)
    _control_logic_register_list[21].name = REG_CURRENT_PRIMARY_AUTO_HOURS_STR;
    _control_logic_register_list[21].address_ptr = &REG_CURRENT_PRIMARY_AUTO_HOURS;
    _control_logic_register_list[21].default_address = REG_CURRENT_PRIMARY_AUTO_HOURS;
    _control_logic_register_list[21].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[22].name = REG_CURRENT_PRIMARY_AUTO_MINUTES_STR;
    _control_logic_register_list[22].address_ptr = &REG_CURRENT_PRIMARY_AUTO_MINUTES;
    _control_logic_register_list[22].default_address = REG_CURRENT_PRIMARY_AUTO_MINUTES;
    _control_logic_register_list[22].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

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

    info(debug_tag, "初始化 LS80 流量控制邏輯 3");
    info(debug_tag, "【診斷】流量限制寄存器地址 - F_HIGH_ALARM=%u, F_LOW_ALARM=%u",
         REG_FLOW_HIGH_LIMIT, REG_FLOW_LOW_LIMIT);

    _register_list_init();

    // ========== 初始化流量限制值 (斷電保持機制) ==========
    info(debug_tag, "【診斷】開始初始化流量限制值...");

    // 讀取最高流量限制
    uint16_t current_high = modbus_read_input_register(REG_FLOW_HIGH_LIMIT);
    info(debug_tag, "【診斷】讀取 F_HIGH_ALARM (46401) = %u (0x%04X)", current_high, current_high);

    if (current_high == 0 || current_high == 0xFFFF) {
        // 寄存器為空或讀取失敗，設置預設值 50.0 LPM (500 * 0.1 = 50.0)
        info(debug_tag, "【診斷】寄存器值無效,嘗試寫入預設值 500...");
        int write_result = modbus_write_single_register(REG_FLOW_HIGH_LIMIT, 500);
        info(debug_tag, "【診斷】寫入結果: %s", write_result == 0 ? "成功" : "失敗");

        // 回讀驗證
        uint16_t verify_high = modbus_read_input_register(REG_FLOW_HIGH_LIMIT);
        info(debug_tag, "【診斷】回讀驗證 F_HIGH_ALARM = %u (預期 500)", verify_high);

        if (verify_high == 500) {
            info(debug_tag, "【開機初始化】設定最高流量限制預設值: 50.0 LPM ✓");
        } else {
            error(debug_tag, "【開機初始化】設定最高流量限制失敗! 回讀值=%u", verify_high);
        }
    } else {
        // 寄存器已有有效值，保留現有設定 (HMI 寫入的值)
        float flow_lpm = current_high / 10.0f;
        info(debug_tag, "【開機初始化】最高流量限制已設置: %.1f LPM (保留現有值) ✓", flow_lpm);
    }

    // 讀取最低流量限制
    uint16_t current_low = modbus_read_input_register(REG_FLOW_LOW_LIMIT);
    info(debug_tag, "【診斷】讀取 F_LOW_ALARM (46402) = %u (0x%04X)", current_low, current_low);

    if (current_low == 0 || current_low == 0xFFFF) {
        // 寄存器為空或讀取失敗，設置預設值 10.0 LPM (100 * 0.1 = 10.0)
        info(debug_tag, "【診斷】寄存器值無效,嘗試寫入預設值 100...");
        int write_result = modbus_write_single_register(REG_FLOW_LOW_LIMIT, 100);
        info(debug_tag, "【診斷】寫入結果: %s", write_result == 0 ? "成功" : "失敗");

        // 回讀驗證
        uint16_t verify_low = modbus_read_input_register(REG_FLOW_LOW_LIMIT);
        info(debug_tag, "【診斷】回讀驗證 F_LOW_ALARM = %u (預期 100)", verify_low);

        if (verify_low == 100) {
            info(debug_tag, "【開機初始化】設定最低流量限制預設值: 10.0 LPM ✓");
        } else {
            error(debug_tag, "【開機初始化】設定最低流量限制失敗! 回讀值=%u", verify_low);
        }
    } else {
        // 寄存器已有有效值，保留現有設定 (HMI 寫入的值)
        float flow_lpm = current_low / 10.0f;
        info(debug_tag, "【開機初始化】最低流量限制已設置: %.1f LPM (保留現有值) ✓", flow_lpm);
    }

    info(debug_tag, "【診斷】流量限制初始化完成");

    // ========== 恢復顯示時間 (斷電保持機制) ==========
    info(debug_tag, "【診斷】開始恢復顯示時間...");

    if (restore_display_time_from_file() == SUCCESS) {
        info(debug_tag, "【開機初始化】顯示時間恢復成功 ✓");
    } else {
        // 文件不存在或損壞,使用預設值 0
        modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_HOURS, 0);
        modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_MINUTES, 0);
        info(debug_tag, "【開機初始化】顯示時間初始化為 0:00 ✓");
    }

    info(debug_tag, "【診斷】顯示時間初始化完成");

    return ret;
}

/**
 * 流量安全檢查函數
 *
 * 檢查流量感測器的值是否超出安全限制範圍
 *
 * @param data 流量感測器數據結構
 * @return 0: 安全, -1: 流量超限 (需要警報)
 */
static int check_flow_limits(const flow_sensor_data_t *data) {
    // 從寄存器讀取流量限制值 (HMI 可設定)
    uint16_t f_high_alarm = modbus_read_input_register(REG_FLOW_HIGH_LIMIT);
    uint16_t f_low_alarm = modbus_read_input_register(REG_FLOW_LOW_LIMIT);

    // 轉換為實際流量值 (0.1 LPM 精度)
    float high_limit_lpm = f_high_alarm / 10.0f;
    float low_limit_lpm = f_low_alarm / 10.0f;

    int alarm_triggered = 0;

    // 檢查 F1 一次側進水流量
    if (data->F1_primary_inlet > high_limit_lpm) {
        error(debug_tag, "【流量警報】F1 一次側進水流量過高: %.1f LPM > %.1f LPM",
              data->F1_primary_inlet, high_limit_lpm);
        alarm_triggered = 1;
    } else if (data->F1_primary_inlet < low_limit_lpm && data->F1_primary_inlet > 0.1f) {
        error(debug_tag, "【流量警報】F1 一次側進水流量過低: %.1f LPM < %.1f LPM",
              data->F1_primary_inlet, low_limit_lpm);
        alarm_triggered = 1;
    }

    // 檢查 F2 二次側出水流量 (主要控制目標)
    if (data->F2_secondary_outlet > high_limit_lpm) {
        error(debug_tag, "【流量警報】F2 二次側出水流量過高: %.1f LPM > %.1f LPM",
              data->F2_secondary_outlet, high_limit_lpm);
        alarm_triggered = 1;
    } else if (data->F2_secondary_outlet < low_limit_lpm && data->F2_secondary_outlet > 0.1f) {
        error(debug_tag, "【流量警報】F2 二次側出水流量過低: %.1f LPM < %.1f LPM",
              data->F2_secondary_outlet, low_limit_lpm);
        alarm_triggered = 1;
    }

    if (alarm_triggered) {
        return -1;  // 流量超限
    }

    return 0;  // 安全
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

    // === 寄存器初始化 (只在第一次執行) ===
    static bool registers_initialized = false;
    if (!registers_initialized) {
        // 如果主泵選擇寄存器未設定,設為預設值 1 (Pump1)
        uint16_t primary_pump = modbus_read_input_register(REG_PRIMARY_PUMP_INDEX);
        if (primary_pump != 1 && primary_pump != 2) {
            modbus_write_single_register(REG_PRIMARY_PUMP_INDEX, 1);
            info(debug_tag, "初始化主泵選擇為 Pump1");
        }

        // 初始化 Pump1 啟用開關 (預設啟用)
        uint16_t pump1_use = modbus_read_input_register(REG_PUMP1_USE);
        if (pump1_use == 0) {
            modbus_write_single_register(REG_PUMP1_USE, 1);
            info(debug_tag, "初始化 Pump1 啟用開關為 1 (啟用)");
        }

        // 初始化 Pump2 啟用開關 (預設啟用)
        uint16_t pump2_use = modbus_read_input_register(REG_PUMP2_USE);
        if (pump2_use == 0) {
            modbus_write_single_register(REG_PUMP2_USE, 1);
            info(debug_tag, "初始化 Pump2 啟用開關為 1 (啟用)");
        }

        // 累計時間寄存器不重置,保留斷電前的值
        info(debug_tag, "系統啟動 - Pump1 AUTO 累計: %d 小時 %d 分鐘",
             modbus_read_input_register(REG_PUMP1_AUTO_MODE_HOURS),
             modbus_read_input_register(REG_PUMP1_AUTO_MODE_MINUTES));
        info(debug_tag, "系統啟動 - Pump2 AUTO 累計: %d 小時 %d 分鐘",
             modbus_read_input_register(REG_PUMP2_AUTO_MODE_HOURS),
             modbus_read_input_register(REG_PUMP2_AUTO_MODE_MINUTES));

        // 初始化主泵變化追蹤
        last_primary_pump_index = modbus_read_input_register(REG_PRIMARY_PUMP_INDEX);
        if (last_primary_pump_index != 1 && last_primary_pump_index != 2) {
            last_primary_pump_index = 1;
        }
        info(debug_tag, "初始化主泵變化追蹤: primary_pump=%d", last_primary_pump_index);

        registers_initialized = true;
    }

    // 處理 AUTO_START_STOP 邊緣觸發
    handle_auto_start_stop();

    // 【步驟0】檢測 enable 從 1 變為 0，觸發切換到手動模式
    uint16_t current_enable = modbus_read_input_register(REG_CONTROL_LOGIC_3_ENABLE);

    // 偵測到 0→1 轉換：保存當前 PUMP_MANUAL_MODE
    if (previous_enable_status == 0 && current_enable == 1) {
        saved_pump1_manual_mode = modbus_read_input_register(REG_PUMP1_MANUAL_MODE);
        saved_pump2_manual_mode = modbus_read_input_register(REG_PUMP2_MANUAL_MODE);
        info(debug_tag, "【ENABLE_3 0→1】保存 PUMP_MANUAL_MODE - P1=%d, P2=%d",
             saved_pump1_manual_mode, saved_pump2_manual_mode);
    }

    if (previous_enable_status == 1 && current_enable == 0) {
        // 保存 PUMP_MANUAL_MODE 狀態 (避免重複保存)
        if (saved_pump1_manual_mode == 0xFFFF) {
            saved_pump1_manual_mode = modbus_read_input_register(REG_PUMP1_MANUAL_MODE);
            saved_pump2_manual_mode = modbus_read_input_register(REG_PUMP2_MANUAL_MODE);
            info(debug_tag, "【ENABLE_3 1→0】保存 PUMP_MANUAL_MODE - P1=%d, P2=%d",
                 saved_pump1_manual_mode, saved_pump2_manual_mode);
        }

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

    // 【步驟4.5】流量限制檢查 (不受 control_logic_ls80_3_enable 影響)
    // 注意: 這個檢查在所有模式下都會執行,確保系統安全
    if (check_flow_limits(&sensor_data) != 0) {
        warn(debug_tag, "流量超出安全限制範圍,請檢查系統!");
        // 注意: 這裡只記錄警告,不強制停機,實際停機邏輯需要由上層決定
    }

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

    // 【步驟5】檢查控制模式 (基於 AUTO_START_STOP)
    // 修改理由: 使用 AUTO_START_STOP 判斷模式,避免與需求(保持 PUMP_MANUAL_MODE 不變)的邏輯矛盾
    // AUTO_START_STOP = 1 → 自動模式 (執行 PID 控制)
    // AUTO_START_STOP = 0 → 手動模式 (僅監控)
    uint16_t auto_start_stop = modbus_read_input_register(REG_AUTO_START_STOP);
    control_mode = (auto_start_stop == 0) ? FLOW_CONTROL_MODE_MANUAL : FLOW_CONTROL_MODE_AUTO;

    info(debug_tag, "控制模式判斷 - AUTO_START_STOP=%d, control_mode=%s",
         auto_start_stop, (control_mode == FLOW_CONTROL_MODE_AUTO) ? "AUTO" : "MANUAL");

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

    // 【步驟7】更新顯示時間累積 (45046/45047)
    // 當 AUTO_START_STOP = 1 時,累積顯示時間
    update_display_auto_time();

    // 【步驟8】檢查並執行主泵自動切換
    // 當顯示時間達到設定值時,切換主泵並歸零顯示時間
    check_and_switch_primary_pump();

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
 * 累積主泵 AUTO 模式運轉時間
 * @param hour_reg 小時寄存器地址
 * @param min_reg 分鐘寄存器地址
 * @param elapsed 經過的秒數
 */
static void accumulate_auto_mode_time(uint32_t hour_reg, uint32_t min_reg, time_t elapsed) {
    if (elapsed <= 0) return;

    uint16_t minutes = modbus_read_input_register(min_reg);
    uint16_t hours = modbus_read_input_register(hour_reg);

    // 累積分鐘
    minutes += (elapsed / 60);

    // 分鐘進位到小時
    if (minutes >= 60) {
        hours += (minutes / 60);
        minutes = minutes % 60;
    }

    // 寫回寄存器
    modbus_write_single_register(min_reg, minutes);
    modbus_write_single_register(hour_reg, hours);
}

/**
 * 更新主泵 AUTO 模式時間 (只在該泵為主泵且 AUTO_START_STOP=1 時累計)
 * @param pump_index 泵浦編號 (1 或 2)
 * @param tracker 追蹤器指標
 * @param hour_reg 小時寄存器
 * @param min_reg 分鐘寄存器
 */
static void update_primary_pump_auto_time(int pump_index,
                                         primary_pump_auto_tracker_t *tracker,
                                         uint32_t hour_reg, uint32_t min_reg) {
    // 讀取當前狀態
    uint16_t auto_start_stop = modbus_read_input_register(REG_AUTO_START_STOP);
    uint16_t current_primary = modbus_read_input_register(REG_PRIMARY_PUMP_INDEX);
    bool is_primary_and_auto = (auto_start_stop == 1) && (current_primary == pump_index);

    time_t current_time = time(NULL);

    // 初始化 - 重啟後保留斷電前的累計時間
    if (!tracker->initialized) {
        tracker->last_update_time = current_time;
        tracker->last_auto_mode_state = is_primary_and_auto;
        tracker->initialized = true;

        // 重啟後第一次,不做任何時間累計,只記錄當前狀態
        debug(debug_tag, "Pump%d AUTO 追蹤器初始化 (保留斷電前累計時間: %d 小時 %d 分鐘)",
              pump_index,
              modbus_read_input_register(hour_reg),
              modbus_read_input_register(min_reg));
        return;
    }

    // 如果該泵為主泵且在 AUTO 模式,累積時間
    if (is_primary_and_auto && tracker->last_auto_mode_state) {
        time_t elapsed = difftime(current_time, tracker->last_update_time);

        if (elapsed >= 60) {  // 每 60 秒更新一次 (1分鐘)
            accumulate_auto_mode_time(hour_reg, min_reg, elapsed);
            tracker->last_update_time = current_time;

            debug(debug_tag, "Pump%d AUTO 時間累計: +%ld 秒, 總計 %d 小時 %d 分鐘",
                  pump_index, elapsed,
                  modbus_read_input_register(hour_reg),
                  modbus_read_input_register(min_reg));
        }
    } else {
        // 狀態改變,更新時間戳
        tracker->last_update_time = current_time;
    }

    // 更新狀態
    tracker->last_auto_mode_state = is_primary_and_auto;
}

/**
 * 保存顯示時間到文件
 *
 * 功能:
 * - 將 REG_CURRENT_PRIMARY_AUTO_HOURS (45046) 和
 *   REG_CURRENT_PRIMARY_AUTO_MINUTES (45047) 保存到 JSON 文件
 * - 使用 fsync 確保數據寫入磁盤
 *
 * @return SUCCESS 成功, FAIL 失敗
 */
static int save_display_time_to_file(void) {
    // 讀取當前顯示時間寄存器
    uint16_t hours = modbus_read_input_register(REG_CURRENT_PRIMARY_AUTO_HOURS);
    uint16_t minutes = modbus_read_input_register(REG_CURRENT_PRIMARY_AUTO_MINUTES);

    // 建立 JSON 對象
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        error(debug_tag, "【斷電保持】建立 JSON 對象失敗");
        return FAIL;
    }

    // 添加時間數據
    cJSON_AddNumberToObject(root, "display_hours", hours);
    cJSON_AddNumberToObject(root, "display_minutes", minutes);
    cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));

    // 轉換為 JSON 字符串
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_string == NULL) {
        error(debug_tag, "【斷電保持】轉換 JSON 字符串失敗");
        return FAIL;
    }

    // 保存到文件
    FILE *fp = fopen(DISPLAY_TIME_PERSIST_FILE, "w");
    if (!fp) {
        error(debug_tag, "【斷電保持】無法打開文件寫入: %s", DISPLAY_TIME_PERSIST_FILE);
        free(json_string);
        return FAIL;
    }

    size_t len = strlen(json_string);
    size_t written = fwrite(json_string, 1, len, fp);

    // 刷新 stdio 緩衝區到內核頁面緩存
    fflush(fp);

    // 強制刷新到存儲設備 (確保斷電後數據不丟失)
    int fd = fileno(fp);
    if (fsync(fd) != 0) {
        warn(debug_tag, "【斷電保持】fsync 失敗,數據可能在斷電時丟失");
    }

    fclose(fp);
    free(json_string);

    if (written != len) {
        error(debug_tag, "【斷電保持】寫入文件失敗: %s", DISPLAY_TIME_PERSIST_FILE);
        return FAIL;
    }

    debug(debug_tag, "【斷電保持】顯示時間已保存: %d 小時 %d 分鐘", hours, minutes);
    return SUCCESS;
}

/**
 * 從文件恢復顯示時間
 *
 * 功能:
 * - 系統啟動時讀取上次保存的顯示時間
 * - 恢復到 REG_CURRENT_PRIMARY_AUTO_HOURS (45046) 和
 *   REG_CURRENT_PRIMARY_AUTO_MINUTES (45047)
 *
 * @return SUCCESS 成功, FAIL 失敗或文件不存在
 */
static int restore_display_time_from_file(void) {
    // 讀取整個文件
    long json_len = 0;
    char *json_text = control_logic_read_entire_file(DISPLAY_TIME_PERSIST_FILE, &json_len);

    if (json_text == NULL) {
        info(debug_tag, "【斷電保持】顯示時間持久化文件不存在,使用預設值 0");
        return FAIL;
    }

    // 解析 JSON
    cJSON *root = cJSON_Parse(json_text);
    free(json_text);

    if (root == NULL) {
        error(debug_tag, "【斷電保持】解析 JSON 失敗,文件可能已損壞");
        remove(DISPLAY_TIME_PERSIST_FILE);  // 刪除損壞文件
        return FAIL;
    }

    // 讀取時間數據
    cJSON *hours_item = cJSON_GetObjectItemCaseSensitive(root, "display_hours");
    cJSON *minutes_item = cJSON_GetObjectItemCaseSensitive(root, "display_minutes");

    if (!cJSON_IsNumber(hours_item) || !cJSON_IsNumber(minutes_item)) {
        error(debug_tag, "【斷電保持】JSON 格式錯誤,缺少必要字段");
        cJSON_Delete(root);
        remove(DISPLAY_TIME_PERSIST_FILE);  // 刪除損壞文件
        return FAIL;
    }

    uint16_t hours = (uint16_t)hours_item->valueint;
    uint16_t minutes = (uint16_t)minutes_item->valueint;

    // 數據驗證 (防止異常值)
    if (minutes >= 60) {
        warn(debug_tag, "【斷電保持】恢復的分鐘數無效: %d, 使用 0", minutes);
        minutes = 0;
    }

    if (hours > 100000) {
        warn(debug_tag, "【斷電保持】恢復的小時數異常大: %d, 使用 0", hours);
        hours = 0;
    }

    // 寫回寄存器
    modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_HOURS, hours);
    modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_MINUTES, minutes);

    info(debug_tag, "【斷電保持】成功恢復顯示時間: %d 小時 %d 分鐘", hours, minutes);

    cJSON_Delete(root);
    return SUCCESS;
}

/**
 * 更新當前主泵 AUTO 累積時間 (顯示寄存器 45046/45047)
 *
 * 功能:
 * - 當 AUTO_START_STOP = 1 時,累積時間到 45046/45047
 * - 與 Pump1/Pump2 各自的累積時間 (42170-42173) 分開計算
 * - 用於 HMI 顯示和切換判斷
 * - 偵測主泵變化,變化時自動歸零顯示時間
 */
static void update_display_auto_time(void) {
    // 讀取 AUTO_START_STOP 狀態
    uint16_t auto_start_stop = modbus_read_input_register(REG_AUTO_START_STOP);
    bool is_auto_mode = (auto_start_stop == 1);

    time_t current_time = time(NULL);

    // 【新增】檢測主泵是否被改變 (HMI 手動修改偵測)
    uint16_t current_primary = modbus_read_input_register(REG_PRIMARY_PUMP_INDEX);
    if (current_primary != 1 && current_primary != 2) {
        current_primary = 1;  // 預設為 Pump1
    }

    if (current_primary != last_primary_pump_index) {
        // HMI 手動修改了主泵!
        info(debug_tag, "偵測到主泵變化: Pump%d -> Pump%d",
             last_primary_pump_index, current_primary);

        // 歸零顯示時間寄存器
        modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_HOURS, 0);
        modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_MINUTES, 0);

        // 重置累積秒數
        display_tracker.accumulated_seconds = 0;

        // 更新時間戳,防止首次累積出現大量秒數
        display_tracker.last_update_time = current_time;

        // 更新追蹤值
        last_primary_pump_index = current_primary;

        info(debug_tag, "主泵顯示時間已歸零 (45046=0, 45047=0)");
        return;  // 本次循環不進行累積,避免包含過渡時的時間
    }

    // 初始化追蹤器
    if (!display_tracker.initialized) {
        display_tracker.last_update_time = current_time;
        display_tracker.last_auto_mode_state = is_auto_mode;
        display_tracker.accumulated_seconds = 0;
        display_tracker.initialized = true;

        info(debug_tag, "顯示時間追蹤器初始化: AUTO=%d, primary_pump=%d",
             is_auto_mode, current_primary);
        return;
    }

    // 只有在 AUTO 模式且持續運行時才累積時間
    if (is_auto_mode && display_tracker.last_auto_mode_state) {
        time_t elapsed = difftime(current_time, display_tracker.last_update_time);

        if (elapsed >= 1) {  // 至少 1 秒
            // 讀取當前累積時間
            uint16_t hours = modbus_read_input_register(REG_CURRENT_PRIMARY_AUTO_HOURS);
            uint16_t minutes = modbus_read_input_register(REG_CURRENT_PRIMARY_AUTO_MINUTES);

            // 累積秒數
            display_tracker.accumulated_seconds += elapsed;

            // 秒進位到分
            if (display_tracker.accumulated_seconds >= 60) {
                minutes += display_tracker.accumulated_seconds / 60;
                display_tracker.accumulated_seconds = display_tracker.accumulated_seconds % 60;
            }

            // 分進位到時
            if (minutes >= 60) {
                hours += minutes / 60;
                minutes = minutes % 60;
            }

            // 寫回寄存器
            modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_HOURS, hours);
            modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_MINUTES, minutes);

            display_tracker.last_update_time = current_time;

            debug(debug_tag, "主泵 Pump%d AUTO時間累積: %d小時%d分%d秒 (累積%ld秒)",
                  current_primary, hours, minutes,
                  display_tracker.accumulated_seconds, (long)elapsed);
        }
    }

    // 更新狀態
    display_tracker.last_auto_mode_state = is_auto_mode;
    display_tracker.last_update_time = current_time;

    // ========== 定期保存顯示時間 ==========
    // 每 5 分鐘保存一次,避免頻繁寫入磁盤
    if (difftime(current_time, last_display_time_save) >= DISPLAY_TIME_SAVE_INTERVAL) {
        if (save_display_time_to_file() == SUCCESS) {
            last_display_time_save = current_time;
        }
    }
}

/**
 * 檢查並執行主泵切換邏輯
 * 根據顯示時間寄存器 (45046/45047) 判斷是否需要切換主泵
 *
 * 切換條件:
 * - REG_CURRENT_PRIMARY_AUTO_HOURS (45046) >= REG_PUMP_SWITCH_HOUR (45034)
 * - 且 REG_CURRENT_PRIMARY_AUTO_MINUTES (45047) = 0
 * - 且 display_tracker.accumulated_seconds <= 1 (精確在整點觸發)
 *
 * 切換動作:
 * 1. 切換主泵編號 (1 ↔ 2)
 * 2. 將 45046/45047 歸零
 * 3. 將 display_tracker.accumulated_seconds 歸零
 * 4. 原始 Pump1/Pump2 累計時間 (42170-42173) 不受影響
 */
static void check_and_switch_primary_pump(void) {
    // 讀取切換時數設定 (0 表示停用自動切換)
    uint16_t switch_hour = modbus_read_input_register(REG_PUMP_SWITCH_HOUR);
    if (switch_hour == 0) {
        return;  // 自動切換功能停用
    }

    // 讀取顯示時間寄存器
    uint16_t display_hours = modbus_read_input_register(REG_CURRENT_PRIMARY_AUTO_HOURS);
    uint16_t display_minutes = modbus_read_input_register(REG_CURRENT_PRIMARY_AUTO_MINUTES);

    // 檢查切換條件:達到設定時數、分鐘為0、秒數<=1
    if (display_hours >= switch_hour &&
        display_minutes == 0 &&
        display_tracker.accumulated_seconds <= 1) {

        // 讀取當前主泵
        uint16_t current_primary = modbus_read_input_register(REG_PRIMARY_PUMP_INDEX);
        if (current_primary != 1 && current_primary != 2) {
            current_primary = 1;  // 預設為 Pump1
        }

        // 切換主泵 (1 ↔ 2)
        uint16_t new_primary = (current_primary == 1) ? 2 : 1;
        modbus_write_single_register(REG_PRIMARY_PUMP_INDEX, new_primary);

        // 歸零顯示時間寄存器
        modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_HOURS, 0);
        modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_MINUTES, 0);

        // 歸零累積秒數
        display_tracker.accumulated_seconds = 0;

        info(debug_tag, "主泵切換: Pump%d -> Pump%d (顯示時間達到 %d 小時 %d 分,設定值 %d 小時)",
             current_primary, new_primary, display_hours, display_minutes, switch_hour);
        info(debug_tag, "顯示時間已歸零,原始 Pump1/Pump2 累計時間不受影響");

        // ========== 立即保存顯示時間 (已歸零) ==========
        save_display_time_to_file();
        last_display_time_save = time(NULL);  // 更新保存時間戳
    }
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

    // === 更新主泵 AUTO 模式時間 ===
    update_primary_pump_auto_time(1, &pump1_auto_tracker,
                                  REG_PUMP1_AUTO_MODE_HOURS,
                                  REG_PUMP1_AUTO_MODE_MINUTES);
    update_primary_pump_auto_time(2, &pump2_auto_tracker,
                                  REG_PUMP2_AUTO_MODE_HOURS,
                                  REG_PUMP2_AUTO_MODE_MINUTES);

    // === 檢查並執行主泵切換 ===
    check_and_switch_primary_pump();

    info(debug_tag, "自動流量控制模式執行 (F2→Fset追蹤)");

    // 設定自動模式
    modbus_write_single_register(REG_FLOW_MODE, 0);         // 0=流量模式
    //modbus_write_single_register(REG_PUMP1_MANUAL_MODE, 0); // 自動模式
    //modbus_write_single_register(REG_PUMP2_MANUAL_MODE, 0);
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

    // === 讀取主泵選擇 ===
    uint16_t primary_pump = modbus_read_input_register(REG_PRIMARY_PUMP_INDEX);
    if (primary_pump != 1 && primary_pump != 2) {
        primary_pump = 1;  // 預設 Pump1
        modbus_write_single_register(REG_PRIMARY_PUMP_INDEX, primary_pump);
    }

    int primary_idx = primary_pump - 1;      // 主泵索引 (0 或 1)
    int secondary_idx = 1 - primary_idx;     // 非輪值主泵索引

    // === 讀取泵浦啟用狀態 ===
    uint16_t pump1_use = modbus_read_input_register(REG_PUMP1_USE);
    uint16_t pump2_use = modbus_read_input_register(REG_PUMP2_USE);
    bool secondary_enabled = (secondary_idx == 0) ? (pump1_use == 1) : (pump2_use == 1);

    // === 讀取非輪值主泵的手動模式狀態 ===
    uint32_t secondary_manual_mode_reg = (secondary_idx == 0) ? REG_PUMP1_MANUAL_MODE : REG_PUMP2_MANUAL_MODE;
    uint32_t secondary_speed_reg = (secondary_idx == 0) ? REG_PUMP1_SPEED : REG_PUMP2_SPEED;
    uint16_t secondary_manual_mode = modbus_read_input_register(secondary_manual_mode_reg);

    float secondary_speed = 30.0f;  // 非輪值主泵預設 30%
    float primary_speed = pid_output;

    // === 策略 1: 死區處理 ===
    if (abs_pid_output < CONTROL_DEADZONE) {
        output->active_pumps[primary_idx] = 1;
        output->pump_speeds[primary_idx] = 30.0f;

        // 檢查非輪值主泵是否為手動模式
        if (secondary_enabled) {
            if (secondary_manual_mode == 1) {
                // 手動模式：讀取手動速度設定值
                uint16_t manual_speed = modbus_read_input_register(secondary_speed_reg);
                output->pump_speeds[secondary_idx] = (float)manual_speed;
                debug(debug_tag, "死區模式: 主泵=Pump%d(30%%), 非輪值=Pump%d(手動 %d%%)",
                      primary_pump, (secondary_idx + 1), manual_speed);
            } else {
                // 自動模式：固定 30%
                output->pump_speeds[secondary_idx] = 30.0f;
                debug(debug_tag, "死區模式: 主泵=Pump%d(30%%), 非輪值=Pump%d(30%%)",
                      primary_pump, (secondary_idx + 1));
            }
            output->active_pumps[secondary_idx] = 1;
        } else {
            output->pump_speeds[secondary_idx] = 0.0f;
            output->active_pumps[secondary_idx] = 0;
            debug(debug_tag, "死區模式: 主泵=Pump%d(30%%), 非輪值=Pump%d(停止)",
                  primary_pump, (secondary_idx + 1));
        }
        return;
    }

    // === 策略 2: 單泵模式 (非輪值主泵停用) ===
    if (!secondary_enabled) {
        output->active_pumps[primary_idx] = 1;
        output->pump_speeds[primary_idx] = fmaxf(pid_output, 30.0f);
        output->active_pumps[secondary_idx] = 0;
        output->pump_speeds[secondary_idx] = 0.0f;

        debug(debug_tag, "單泵模式: 主泵=Pump%d(%.1f%%), 非輪值=Pump%d(停用)",
              primary_pump, output->pump_speeds[primary_idx], (secondary_idx + 1));
        return;
    }

    // === 策略 3: 非輪值主泵手動模式 ===
    if (secondary_manual_mode == 1) {
        uint16_t manual_speed = modbus_read_input_register(secondary_speed_reg);
        secondary_speed = (float)manual_speed;
        primary_speed = pid_output - secondary_speed;

        debug(debug_tag, "手動模式: 非輪值=Pump%d(%.1f%% 手動), 主泵=Pump%d(%.1f%%)",
              (secondary_idx + 1), secondary_speed, primary_pump, primary_speed);
    }
    // === 策略 4: 雙泵自動模式 ===
    else {
        secondary_speed = 30.0f;
        //primary_speed = pid_output - 30.0f;
        primary_speed = pid_output;

        debug(debug_tag, "雙泵自動: 非輪值=Pump%d(30%%), 主泵=Pump%d(%.1f%%)",
              (secondary_idx + 1), primary_pump, primary_speed);
    }

    // 確保主泵速度 ≥ 30%
    if (primary_speed < 30.0f) {
        primary_speed = 30.0f;
    }

    // 確保主泵速度 ≤ 100%
    if (primary_speed > 100.0f) {
        primary_speed = 100.0f;
    }

    // 寫入輸出
    output->active_pumps[primary_idx] = 1;
    output->pump_speeds[primary_idx] = primary_speed;
    output->active_pumps[secondary_idx] = 1;
    output->pump_speeds[secondary_idx] = secondary_speed;

    debug(debug_tag, "泵浦控制計算完成 - PID: %.1f%%, 主泵Pump%d=%.1f%%, 非輪值Pump%d=%.1f%%",
          pid_output, primary_pump, primary_speed, (secondary_idx + 1), secondary_speed);
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

}   

/*
 * control_logic_ls80_2_m_v01.c - LS80 壓力差控制邏輯（手動替換版本 v01）
 *
 * ============================================================================
 * 【版本說明】
 * ============================================================================
 * - 檔案名稱：control_logic_ls80_2_m_v01.c（手動替換版本）
 * - 替換目標：control_logic_ls80_2.c
 * - 部署方式：手動改名為 control_logic_ls80_2.c 後執行 ./build_kenmec.sh 編譯
 * - 版本：v01
 * - 日期：2025
 *
 * ============================================================================
 * 【功能概述】
 * ============================================================================
 * 本模組實現 CDU 系統的壓力差控制功能，通過 PID 演算法維持冷卻水系統壓力差穩定
 * 支援 (P4-P2)→Pset 追蹤模式，並提供2泵協調控制策略，確保壓力差精確跟隨設定值
 *
 * ============================================================================
 * 【控制目標】
 * ============================================================================
 * - 維持二次側壓力差 (P4進水 - P2出水) 追蹤設定值 Pset
 * - 預設目標壓差：REG_PRESSURE_SETPOINT (45002)
 * - 追蹤模式：(P4-P2)→Pset
 *
 * ============================================================================
 * 【與原版差異】
 * ============================================================================
 * 原版 control_logic_ls80_2.c：
 * - 計算 P1-P3 壓力差（一次側）
 * - 使用 411xxx 硬體地址
 * - 泵速寫入：speed × 10 × 10 = mV (0-10000mV)
 *
 * 新版 control_logic_ls80_2_m_v01.c：
 * - 計算 P4-P2 壓力差（二次側）
 * - 使用 42xxx 映射地址（參照 ls80_3.c 流量控制）
 * - 泵速寫入：直接寫入 0-100%（簡化方式）
 * - 新增：P1 和 P3 壓力監控顯示
 *
 * ============================================================================
 * 【感測器配置】
 * ============================================================================
 * - P1 (42082): 一次側進水壓力（監控顯示）0.01 bar 精度
 * - P2 (42083): 二次側出水壓力（控制目標）0.01 bar 精度
 * - P3 (42084): 一次側出水壓力（監控顯示）0.01 bar 精度
 * - P4 (42085): 二次側進水壓力（控制目標）0.01 bar 精度
 *
 * ============================================================================
 * 【執行器控制】
 * ============================================================================
 * - Pump1/2: 泵浦速度 0-100% (REG 45015/45016)
 * - Pump1/2啟停: DO 控制 (REG 411101/411103)
 *
 * ============================================================================
 * 【控制模式】
 * ============================================================================
 * - 手動模式：僅監控，接受外部設定泵速，不干預控制
 * - 自動模式：PID 控制 + 2泵協調策略
 *
 * ============================================================================
 * 【PID 參數】
 * ============================================================================
 * - Kp: 2.0（比例增益）
 * - Ki: 0.5（積分增益）
 * - Kd: 0.1（微分增益）
 * - 輸出範圍: 0% ~ 100%
 * - 積分限幅: 防止飽和（±output_max/Ki）
 *
 * ============================================================================
 * 【JSON 配置需求】
 * ============================================================================
 * 需要配置 /usrdata/analog_input_current_configs：
 * [
 *   {"board": 0, "channel": 0, "sensor_type": 1, "update_address": 2082, "name": "P1"},
 *   {"board": 0, "channel": 2, "sensor_type": 1, "update_address": 2083, "name": "P2"},
 *   {"board": 0, "channel": 1, "sensor_type": 1, "update_address": 2084, "name": "P3"},
 *   {"board": 0, "channel": 3, "sensor_type": 1, "update_address": 2085, "name": "P4"}
 * ]
 *
 * 部署配置指令（使用 Redfish API）：
 * curl -X POST "http://<device-ip>:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write" \
 *   -H "Content-Type: application/json" \
 *   -d '{
 *     "AnalogInputCurrentConfigs": [
 *       {"board": 0, "channel": 0, "sensor_type": 1, "update_address": 2082, "name": "P1"},
 *       {"board": 0, "channel": 2, "sensor_type": 1, "update_address": 2083, "name": "P2"},
 *       {"board": 0, "channel": 1, "sensor_type": 1, "update_address": 2084, "name": "P3"},
 *       {"board": 0, "channel": 3, "sensor_type": 1, "update_address": 2085, "name": "P4"}
 *     ]
 *   }'
 *
 * ============================================================================
 * 【安全機制】
 * ============================================================================
 * 本版本暫不實施安全機制，僅實現基本 PID 控制和手動/自動模式切換。
 * 未來版本可增加：
 * - 壓力上下限檢查
 * - 壓力差超限警報
 * - 緊急停機邏輯
 *
 * 作者: Claude AI (基於 control_logic_ls80_3.c 架構)
 * 日期: 2025
 * 版本: v01
 */

#include "dexatek/main_application/include/application_common.h"
#include "kenmec/main_application/control_logic/control_logic_manager.h"

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
static const char *debug_tag = "ls80_2_m_v01";

#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_ls80_2.json"
#define CONFIG_REGISTER_LIST_SIZE 25
static control_logic_register_t _control_logic_register_list[CONFIG_REGISTER_LIST_SIZE];

// ========== 系統控制 ==========
static uint32_t REG_CONTROL_LOGIC_2_ENABLE = 41002; // 控制邏輯2啟用
static uint32_t REG_CONTROL_LOGIC_3_ENABLE = 41003; // 控制邏輯3啟用
static uint32_t REG_AUTO_START_STOP = 45020;        // 自動啟停開關

// ========== 壓力感測器（使用 42xxx 映射地址）==========
static uint32_t REG_P1_PRESSURE = 42082;  // P1一次側進水壓力（監控）42082
static uint32_t REG_P2_PRESSURE = 42083;  // P2二次側出水壓力（控制）
static uint32_t REG_P3_PRESSURE = 42084;  // P3一次側出水壓力（監控）42084
static uint32_t REG_P4_PRESSURE = 42085;  // P4二次側進水壓力（控制）

// ========== 控制設定（45xxx）==========
static uint32_t REG_PRESSURE_SETPOINT = 45002;  // 壓差設定值（0.1 bar精度）
static uint32_t REG_CONTROL_MODE = 45005;       // 控制模式（0=流量, 1=壓差）

// ========== 泵浦控制（45xxx 速度 + 411xxx 啟停）==========
static uint32_t REG_PUMP1_SPEED = 45015;       // Pump1速度設定 (0-100%)
static uint32_t REG_PUMP2_SPEED = 45016;       // Pump2速度設定 (0-100%)
static uint32_t REG_PUMP1_CONTROL = 411101;    // Pump1啟停控制
static uint32_t REG_PUMP2_CONTROL = 411103;    // Pump2啟停控制

// ========== 手動模式（45xxx）==========
static uint32_t REG_PUMP1_MANUAL_MODE = 45021; // Pump1手動模式 (0=自動, 1=手動)
static uint32_t REG_PUMP2_MANUAL_MODE = 45022; // Pump2手動模式

// ========== 壓力限制（46xxx）==========
static uint32_t REG_P_HIGH_ALARM = 46201;      // 最高壓力限制（預設 5.0 Bar）
static uint32_t REG_P_LOW_ALARM = 46202;       // 最低壓力限制（預設 0.5 Bar）

// ========== 主泵輪換相關寄存器（與 ls80_3.c 共享）==========
static uint32_t REG_PUMP_SWITCH_HOUR = 45034;      // 主泵切換時數設定 (小時, 0=停用自動切換)
static uint32_t REG_PUMP1_USE = 45036;             // Pump1 啟用開關 (0=停用, 1=啟用)
static uint32_t REG_PUMP2_USE = 45037;             // Pump2 啟用開關 (0=停用, 1=啟用)
static uint32_t REG_PRIMARY_PUMP_INDEX = 45045;    // 當前主泵編號 (1=Pump1, 2=Pump2) - HMI 可指定

// 當前主泵 AUTO 模式累積時間顯示寄存器（獨立累積，用於顯示和切換判斷）
static uint32_t REG_CURRENT_PRIMARY_AUTO_HOURS = 45046;    // 顯示用累積小時
static uint32_t REG_CURRENT_PRIMARY_AUTO_MINUTES = 45047;  // 顯示用累積分鐘

// AUTO 模式累計時間寄存器（斷電保持）
static uint32_t REG_PUMP1_AUTO_MODE_HOURS = 42170;    // Pump1 作為主泵在 AUTO 模式累計時間 (小時)
static uint32_t REG_PUMP2_AUTO_MODE_HOURS = 42171;    // Pump2 作為主泵在 AUTO 模式累計時間 (小時)
static uint32_t REG_PUMP1_AUTO_MODE_MINUTES = 42172;  // Pump1 AUTO 模式累計時間 (分鐘)
static uint32_t REG_PUMP2_AUTO_MODE_MINUTES = 42173;  // Pump2 AUTO 模式累計時間 (分鐘)

// ========== 控制參數 ==========
#define PUMP_MIN_SPEED            30.0f   // 泵浦最小速度 % (改為30%)
#define PUMP_MAX_SPEED            100.0f  // 泵浦最大速度 %
#define CONTROL_DEADZONE          5.0f    // 控制死區 % (改為5%)

// ========== 顯示時間持久化配置（與 ls80_3.c 共享機制）==========
#define DISPLAY_TIME_PERSIST_FILE "/usrdata/ls80_2_display_time.json"
#define DISPLAY_TIME_SAVE_INTERVAL 300  // 每 5 分鐘保存一次 (秒)

/*---------------------------------------------------------------------------
                                Variables
 ---------------------------------------------------------------------------*/

typedef enum {
    PRESSURE_CONTROL_MODE_MANUAL = 0,
    PRESSURE_CONTROL_MODE_AUTO = 1
} pressure_control_mode_t;

// 壓力感測器數據結構
typedef struct {
    float P1_primary_inlet;       // P1一次側進水壓力（監控）
    float P2_secondary_outlet;    // P2二次側出水壓力（控制）
    float P3_primary_outlet;      // P3一次側出水壓力（監控）
    float P4_secondary_inlet;     // P4二次側進水壓力（控制）
    float pressure_differential;  // 壓力差 (P4 - P2)
    time_t timestamp;
} pressure_sensor_data_t;

// PID 控制器結構
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

// 泵浦控制輸出結構
typedef struct {
    int active_pumps[2];      // 泵浦啟用狀態
    float pump_speeds[2];     // 泵浦速度 0-100%
} pump_control_output_t;

// PID 控制器初始化（全局變數）
static pressure_pid_controller_t pressure_pid = {
    .kp = 2.5f,               // 比例增益（優化：從 2.0 → 2.5，參照 ls80_3）
    .ki = 0.4f,               // 積分增益（優化：從 0.5 → 0.4，增大積分限幅上限到 250）
    .kd = 0.8f,               // 微分增益（優化：從 0.1 → 0.8，增強阻尼）
    .integral = 0.0f,
    .previous_error = 0.0f,
    .previous_time = 0,
    .output_min = 0.0f,
    .output_max = 100.0f
};

// ========== 主泵 AUTO 模式時間追蹤結構 ==========
typedef struct {
    time_t last_update_time;       // 上次更新時間戳
    bool last_auto_mode_state;     // 上次 AUTO 模式狀態
    bool initialized;              // 是否已初始化
} primary_pump_auto_tracker_t;

// 顯示時間追蹤結構（與 ls80_3.c 相同）
typedef struct {
    uint32_t accumulated_seconds;  // 累積秒數
    bool last_auto_mode_state;     // 上次 AUTO 模式狀態
    bool initialized;              // 是否已初始化
    time_t last_update_time;       // 上次更新時間戳
} display_time_tracker_t;

// 全局追蹤器
static primary_pump_auto_tracker_t pump1_auto_tracker = {0, false, false};
static primary_pump_auto_tracker_t pump2_auto_tracker = {0, false, false};
static display_time_tracker_t display_tracker = {0, false, false, 0};
static uint16_t last_primary_pump_index = 0;
static time_t last_display_time_save = 0;  // 上次保存時間戳

// 追蹤控制邏輯啟用狀態，用於偵測 1→0 轉換
static uint16_t previous_control_logic2_enable = 1;

// 追蹤 AUTO_START_STOP 和 FLOW_MODE 狀態，用於邊緣觸發檢測
static uint16_t previous_auto_start_stop = 0;
static uint16_t previous_flow_mode = 0;  // REG_CONTROL_MODE (45005) 即為 FLOW_MODE

// 追蹤 PUMP_MANUAL_MODE 狀態，用於 FLOW_MODE 切換時保持不變
static uint16_t saved_pump1_manual_mode = 0xFFFF;  // 0xFFFF 表示未初始化
static uint16_t saved_pump2_manual_mode = 0xFFFF;

/*---------------------------------------------------------------------------
                        Function Declarations
 ---------------------------------------------------------------------------*/
// ========== 外部函數宣告 ==========
extern char* control_logic_read_entire_file(const char *path, long *out_len);

static int read_pressure_sensor_data(pressure_sensor_data_t *data);
static float calculate_pressure_pid_output(pressure_pid_controller_t *pid, float setpoint, float current_value);
static void reset_pressure_pid_controller(pressure_pid_controller_t *pid);
static int execute_manual_pressure_control(float target_pressure_diff);
static int execute_automatic_pressure_control(const pressure_sensor_data_t *data);
static void calculate_pump_control(float pid_output, pump_control_output_t *output);
static void execute_pump_control_output(const pump_control_output_t *output);
static void handle_auto_start_stop_and_flow_mode(void);
static void restore_pump_manual_mode_if_saved(void);

// ========== 主泵輪換相關函數宣告 ==========
static void accumulate_auto_mode_time(uint32_t hour_reg, uint32_t min_reg, time_t elapsed);
static void update_primary_pump_auto_time(int pump_index, primary_pump_auto_tracker_t *tracker,
                                         uint32_t hour_reg, uint32_t min_reg);
static void update_display_auto_time(void);
static void check_and_switch_primary_pump(void);
static int save_display_time_to_file(void);
static int restore_display_time_from_file(void);

/*---------------------------------------------------------------------------
                            Implementation
 ---------------------------------------------------------------------------*/

/**
 * Modbus 寄存器讀取（參照 ls80_3.c）
 */
static uint16_t modbus_read_input_register(uint32_t address) {
    uint16_t value = 0;
    int ret = control_logic_read_holding_register(address, &value);

    if (ret != SUCCESS) {
        value = 0xFFFF;
    }

    return value;
}

/**
 * Modbus 寄存器寫入（參照 ls80_3.c）
 */
static bool modbus_write_single_register(uint32_t address, uint16_t value) {
    int ret = control_logic_write_register(address, value, 2000);
    return (ret == SUCCESS) ? true : false;
}

/**
 * 切換到手動模式並保存最後速度值
 *
 * 當 control_logic_2 從啟用變為停用時調用
 */
static void switch_to_manual_mode_with_last_speed(void) {
    info(debug_tag, "control_logic_2 停用，切換到手動模式...");

    // 1. 讀取當前泵浦速度值（最後的速度）
    uint16_t pump1_speed = modbus_read_input_register(REG_PUMP1_SPEED);
    uint16_t pump2_speed = modbus_read_input_register(REG_PUMP2_SPEED);

    info(debug_tag, "保存最後速度值 - Pump1: %d, Pump2: %d",
         pump1_speed, pump2_speed);

    // 2. 重新寫入速度值（確保保持當前速度）
    modbus_write_single_register(REG_PUMP1_SPEED, pump1_speed);
    modbus_write_single_register(REG_PUMP2_SPEED, pump2_speed);

    // 3. 設定為手動模式
    //modbus_write_single_register(REG_PUMP1_MANUAL_MODE, 1);
    //modbus_write_single_register(REG_PUMP2_MANUAL_MODE, 1);

    info(debug_tag, "已切換到手動模式 - Pump1=%d, Pump2=%d (手動模式=1)",
         pump1_speed, pump2_speed);
}

/*---------------------------------------------------------------------------
            AUTO_START_STOP and FLOW_MODE Edge Trigger Handler
 ---------------------------------------------------------------------------*/

/**
 * 處理 AUTO_START_STOP 與 FLOW_MODE 寄存器的聯動控制
 *
 * 【需求1A】FLOW_MODE 0→1 切換 (在 AUTO_START_STOP=1 時)
 * - 條件: AUTO_START_STOP=1 且 FLOW_MODE 從 0→1
 * - 動作: ENABLE_3=0, ENABLE_2=1
 *
 * 【需求1B】FLOW_MODE 1→0 切換 (在 AUTO_START_STOP=1 時)
 * - 條件: AUTO_START_STOP=1 且 FLOW_MODE 從 1→0
 * - 動作: ENABLE_3=1, ENABLE_2=0
 *
 * 【需求2】AUTO_START_STOP=0 時持續檢查
 * - 條件: AUTO_START_STOP 的狀態為 0
 * - 動作: 持續強制 ENABLE_3=0, ENABLE_2=0
 *
 * 【需求3A】AUTO_START_STOP 0→1 (處理 FLOW_MODE=1 情況)
 * - 條件: AUTO_START_STOP 從 0→1 且 FLOW_MODE=1
 * - 動作: ENABLE_2=1
 *
 * 【需求3B】AUTO_START_STOP 1→0
 * - 條件: AUTO_START_STOP 從 1→0 且 ENABLE_2=1
 * - 動作: ENABLE_2=0
 */
static void handle_auto_start_stop_and_flow_mode(void) {
    // 讀取當前寄存器狀態
    uint16_t current_auto_start_stop = modbus_read_input_register(REG_AUTO_START_STOP);
    uint16_t current_flow_mode = modbus_read_input_register(REG_CONTROL_MODE);  // 45005 FLOW_MODE

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

    // 【需求3A】AUTO_START_STOP 0→1 邊緣觸發
    if (previous_auto_start_stop == 0 && current_auto_start_stop == 1) {
        if (current_flow_mode == 1) {
            // FLOW_MODE=1 (壓差模式) → 啟用 ENABLE_2
            bool success = modbus_write_single_register(REG_CONTROL_LOGIC_2_ENABLE, 1);
            if (success) {
                info(debug_tag, "【需求3A】AUTO_START_STOP 0→1 且 FLOW_MODE=1 → ENABLE_2=1");
            }
        }
        // 注意: FLOW_MODE=0 的情況由 ls80_3.c 處理 (舊邏輯保留)
    }

    // 【需求3B】AUTO_START_STOP 1→0 邊緣觸發
    if (previous_auto_start_stop == 1 && current_auto_start_stop == 0) {
        uint16_t enable_2 = modbus_read_input_register(REG_CONTROL_LOGIC_2_ENABLE);

        if (enable_2 == 1) {
            bool success = modbus_write_single_register(REG_CONTROL_LOGIC_2_ENABLE, 0);
            if (success) {
                info(debug_tag, "【需求3B】AUTO_START_STOP 1→0 且 ENABLE_2=1 → ENABLE_2=0");
            }
        }
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
 * 在 control_logic_ls80_2_pressure_control() 主函數的最後調用
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

/**
 * Register 列表初始化
 */
static int _register_list_init(void)
{
    int ret = SUCCESS;

    // 控制邏輯2啟用
    _control_logic_register_list[0].name = REG_CONTROL_LOGIC_2_ENABLE_STR;
    _control_logic_register_list[0].address_ptr = &REG_CONTROL_LOGIC_2_ENABLE;
    _control_logic_register_list[0].default_address = REG_CONTROL_LOGIC_2_ENABLE;
    _control_logic_register_list[0].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // P1 壓力
    _control_logic_register_list[1].name = REG_P1_PRESSURE_STR;
    _control_logic_register_list[1].address_ptr = &REG_P1_PRESSURE;
    _control_logic_register_list[1].default_address = REG_P1_PRESSURE;
    _control_logic_register_list[1].type = CONTROL_LOGIC_REGISTER_READ;

    // P2 壓力
    _control_logic_register_list[2].name = REG_P2_PRESSURE_STR;
    _control_logic_register_list[2].address_ptr = &REG_P2_PRESSURE;
    _control_logic_register_list[2].default_address = REG_P2_PRESSURE;
    _control_logic_register_list[2].type = CONTROL_LOGIC_REGISTER_READ;

    // P3 壓力
    _control_logic_register_list[3].name = REG_P3_PRESSURE_STR;
    _control_logic_register_list[3].address_ptr = &REG_P3_PRESSURE;
    _control_logic_register_list[3].default_address = REG_P3_PRESSURE;
    _control_logic_register_list[3].type = CONTROL_LOGIC_REGISTER_READ;

    // P4 壓力
    _control_logic_register_list[4].name = REG_P4_PRESSURE_STR;
    _control_logic_register_list[4].address_ptr = &REG_P4_PRESSURE;
    _control_logic_register_list[4].default_address = REG_P4_PRESSURE;
    _control_logic_register_list[4].type = CONTROL_LOGIC_REGISTER_READ;

    // 壓差設定值
    _control_logic_register_list[5].name = REG_PRESSURE_SETPOINT_STR;
    _control_logic_register_list[5].address_ptr = &REG_PRESSURE_SETPOINT;
    _control_logic_register_list[5].default_address = REG_PRESSURE_SETPOINT;
    _control_logic_register_list[5].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // 控制模式
    _control_logic_register_list[6].name = REG_FLOW_MODE_STR;
    _control_logic_register_list[6].address_ptr = &REG_CONTROL_MODE;
    _control_logic_register_list[6].default_address = REG_CONTROL_MODE;
    _control_logic_register_list[6].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // Pump1 速度
    _control_logic_register_list[7].name = REG_PUMP1_SPEED_STR;
    _control_logic_register_list[7].address_ptr = &REG_PUMP1_SPEED;
    _control_logic_register_list[7].default_address = REG_PUMP1_SPEED;
    _control_logic_register_list[7].type = CONTROL_LOGIC_REGISTER_WRITE;

    // Pump2 速度
    _control_logic_register_list[8].name = REG_PUMP2_SPEED_STR;
    _control_logic_register_list[8].address_ptr = &REG_PUMP2_SPEED;
    _control_logic_register_list[8].default_address = REG_PUMP2_SPEED;
    _control_logic_register_list[8].type = CONTROL_LOGIC_REGISTER_WRITE;

    // Pump1 控制
    _control_logic_register_list[9].name = REG_PUMP1_CONTROL_STR;
    _control_logic_register_list[9].address_ptr = &REG_PUMP1_CONTROL;
    _control_logic_register_list[9].default_address = REG_PUMP1_CONTROL;
    _control_logic_register_list[9].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // Pump2 控制
    _control_logic_register_list[10].name = REG_PUMP2_CONTROL_STR;
    _control_logic_register_list[10].address_ptr = &REG_PUMP2_CONTROL;
    _control_logic_register_list[10].default_address = REG_PUMP2_CONTROL;
    _control_logic_register_list[10].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // Pump1 手動模式
    _control_logic_register_list[11].name = REG_PUMP1_MANUAL_MODE_STR;
    _control_logic_register_list[11].address_ptr = &REG_PUMP1_MANUAL_MODE;
    _control_logic_register_list[11].default_address = REG_PUMP1_MANUAL_MODE;
    _control_logic_register_list[11].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // Pump2 手動模式
    _control_logic_register_list[12].name = REG_PUMP2_MANUAL_MODE_STR;
    _control_logic_register_list[12].address_ptr = &REG_PUMP2_MANUAL_MODE;
    _control_logic_register_list[12].default_address = REG_PUMP2_MANUAL_MODE;
    _control_logic_register_list[12].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // 最高壓力限制
    _control_logic_register_list[13].name = REG_P_HIGH_ALARM_STR;
    _control_logic_register_list[13].address_ptr = &REG_P_HIGH_ALARM;
    _control_logic_register_list[13].default_address = REG_P_HIGH_ALARM;
    _control_logic_register_list[13].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // 最低壓力限制
    _control_logic_register_list[14].name = REG_P_LOW_ALARM_STR;
    _control_logic_register_list[14].address_ptr = &REG_P_LOW_ALARM;
    _control_logic_register_list[14].default_address = REG_P_LOW_ALARM;
    _control_logic_register_list[14].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // 從配置檔案載入
    uint32_t list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    ret = control_logic_register_load_from_file(CONFIG_REGISTER_FILE_PATH, _control_logic_register_list, list_size);
    debug(debug_tag, "load register array from file %s, ret %d", CONFIG_REGISTER_FILE_PATH, ret);

    return ret;
}

/**
 * 取得配置
 */
int control_logic_ls80_2_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

/**
 * 初始化函數
 */
int control_logic_ls80_2_pressure_control_init(void)
{
    int ret = SUCCESS;

    info(debug_tag, "初始化 LS80 壓力差控制邏輯 2 (手動版本 v01)");
    info(debug_tag, "【診斷】壓力限制寄存器地址 - P_HIGH_ALARM=%u, P_LOW_ALARM=%u",
         REG_P_HIGH_ALARM, REG_P_LOW_ALARM);

    _register_list_init();

    // ========== 初始化壓力限制值 (斷電保持機制) ==========
    info(debug_tag, "【診斷】開始初始化壓力限制值...");

    // 讀取最高壓力限制
    uint16_t current_high = modbus_read_input_register(REG_P_HIGH_ALARM);
    info(debug_tag, "【診斷】讀取 P_HIGH_ALARM (46201) = %u (0x%04X)", current_high, current_high);

    if (current_high == 0 || current_high == 0xFFFF) {
        // 寄存器為空或讀取失敗，設置預設值 5.0 Bar (500 * 0.01 = 5.0)
        info(debug_tag, "【診斷】寄存器值無效,嘗試寫入預設值 500...");
        int write_result = modbus_write_single_register(REG_P_HIGH_ALARM, 500);
        info(debug_tag, "【診斷】寫入結果: %s", write_result == 0 ? "成功" : "失敗");

        // 回讀驗證
        uint16_t verify_high = modbus_read_input_register(REG_P_HIGH_ALARM);
        info(debug_tag, "【診斷】回讀驗證 P_HIGH_ALARM = %u (預期 500)", verify_high);

        if (verify_high == 500) {
            info(debug_tag, "【開機初始化】設定最高壓力限制預設值: 5.0 Bar ✓");
        } else {
            error(debug_tag, "【開機初始化】設定最高壓力限制失敗! 回讀值=%u", verify_high);
        }
    } else {
        // 寄存器已有有效值，保留現有設定 (HMI 寫入的值)
        float pressure_bar = current_high / 100.0f;
        info(debug_tag, "【開機初始化】最高壓力限制已設置: %.2f Bar (保留現有值) ✓", pressure_bar);
    }

    // 讀取最低壓力限制
    uint16_t current_low = modbus_read_input_register(REG_P_LOW_ALARM);
    info(debug_tag, "【診斷】讀取 P_LOW_ALARM (46202) = %u (0x%04X)", current_low, current_low);

    if (current_low == 0 || current_low == 0xFFFF) {
        // 寄存器為空或讀取失敗，設置預設值 0.5 Bar (50 * 0.01 = 0.5)
        info(debug_tag, "【診斷】寄存器值無效,嘗試寫入預設值 50...");
        int write_result = modbus_write_single_register(REG_P_LOW_ALARM, 50);
        info(debug_tag, "【診斷】寫入結果: %s", write_result == 0 ? "成功" : "失敗");

        // 回讀驗證
        uint16_t verify_low = modbus_read_input_register(REG_P_LOW_ALARM);
        info(debug_tag, "【診斷】回讀驗證 P_LOW_ALARM = %u (預期 50)", verify_low);

        if (verify_low == 50) {
            info(debug_tag, "【開機初始化】設定最低壓力限制預設值: 0.5 Bar ✓");
        } else {
            error(debug_tag, "【開機初始化】設定最低壓力限制失敗! 回讀值=%u", verify_low);
        }
    } else {
        // 寄存器已有有效值，保留現有設定 (HMI 寫入的值)
        float pressure_bar = current_low / 100.0f;
        info(debug_tag, "【開機初始化】最低壓力限制已設置: %.2f Bar (保留現有值) ✓", pressure_bar);
    }

    info(debug_tag, "【診斷】壓力限制初始化完成");

    return ret;
}

/**
 * 壓力安全檢查函數
 *
 * 檢查所有壓力感測器的值是否超出安全限制範圍
 *
 * @param data 壓力感測器數據結構
 * @return 0: 安全, -1: 壓力超限 (需要警報或停機)
 */
static int check_pressure_limits(const pressure_sensor_data_t *data) {
    // 從寄存器讀取壓力限制值 (HMI 可設定)
    uint16_t p_high_alarm = modbus_read_input_register(REG_P_HIGH_ALARM);
    uint16_t p_low_alarm = modbus_read_input_register(REG_P_LOW_ALARM);

    // 轉換為實際壓力值 (0.01 bar 精度)
    float high_limit_bar = p_high_alarm / 100.0f;
    float low_limit_bar = p_low_alarm / 100.0f;

    int alarm_triggered = 0;

    // 檢查 P1 一次側進水壓力
    if (data->P1_primary_inlet > high_limit_bar) {
        error(debug_tag, "【壓力警報】P1 一次側進水壓力過高: %.2f Bar > %.2f Bar",
              data->P1_primary_inlet, high_limit_bar);
        alarm_triggered = 1;
    } else if (data->P1_primary_inlet < low_limit_bar && data->P1_primary_inlet > 0.01f) {
        error(debug_tag, "【壓力警報】P1 一次側進水壓力過低: %.2f Bar < %.2f Bar",
              data->P1_primary_inlet, low_limit_bar);
        alarm_triggered = 1;
    }

    // 檢查 P2 二次側出水壓力 (控制目標)
    if (data->P2_secondary_outlet > high_limit_bar) {
        error(debug_tag, "【壓力警報】P2 二次側出水壓力過高: %.2f Bar > %.2f Bar",
              data->P2_secondary_outlet, high_limit_bar);
        alarm_triggered = 1;
    } else if (data->P2_secondary_outlet < low_limit_bar && data->P2_secondary_outlet > 0.01f) {
        error(debug_tag, "【壓力警報】P2 二次側出水壓力過低: %.2f Bar < %.2f Bar",
              data->P2_secondary_outlet, low_limit_bar);
        alarm_triggered = 1;
    }

    // 檢查 P3 一次側出水壓力
    if (data->P3_primary_outlet > high_limit_bar) {
        error(debug_tag, "【壓力警報】P3 一次側出水壓力過高: %.2f Bar > %.2f Bar",
              data->P3_primary_outlet, high_limit_bar);
        alarm_triggered = 1;
    } else if (data->P3_primary_outlet < low_limit_bar && data->P3_primary_outlet > 0.01f) {
        error(debug_tag, "【壓力警報】P3 一次側出水壓力過低: %.2f Bar < %.2f Bar",
              data->P3_primary_outlet, low_limit_bar);
        alarm_triggered = 1;
    }

    // 檢查 P4 二次側進水壓力 (控制目標)
    if (data->P4_secondary_inlet > high_limit_bar) {
        error(debug_tag, "【壓力警報】P4 二次側進水壓力過高: %.2f Bar > %.2f Bar",
              data->P4_secondary_inlet, high_limit_bar);
        alarm_triggered = 1;
    } else if (data->P4_secondary_inlet < low_limit_bar && data->P4_secondary_inlet > 0.01f) {
        error(debug_tag, "【壓力警報】P4 二次側進水壓力過低: %.2f Bar < %.2f Bar",
              data->P4_secondary_inlet, low_limit_bar);
        alarm_triggered = 1;
    }

    if (alarm_triggered) {
        return -1;  // 壓力超限
    }

    return 0;  // 安全
}

/**
 * 讀取所有壓力感測器數據
 */
static int read_pressure_sensor_data(pressure_sensor_data_t *data) {
    int pressure_raw;

    // 讀取 P1 一次側進水壓力（監控顯示）
    pressure_raw = modbus_read_input_register(REG_P1_PRESSURE);
    if (pressure_raw >= 0 && pressure_raw != 0xFFFF) {
        data->P1_primary_inlet = pressure_raw / 100.0f;  // 0.01 bar精度
    } else {
        warn(debug_tag, "P1壓力讀取失敗");
        data->P1_primary_inlet = 0.0f;
    }

    // 讀取 P2 二次側出水壓力（控制目標）
    pressure_raw = modbus_read_input_register(REG_P2_PRESSURE);
    if (pressure_raw >= 0 && pressure_raw != 0xFFFF) {
        data->P2_secondary_outlet = pressure_raw / 100.0f;  // 0.01 bar精度
    } else {
        error(debug_tag, "P2壓力讀取失敗 - 這是主要控制目標！");
        data->P2_secondary_outlet = 0.0f;
    }

    // 讀取 P3 一次側出水壓力（監控顯示）
    pressure_raw = modbus_read_input_register(REG_P3_PRESSURE);
    if (pressure_raw >= 0 && pressure_raw != 0xFFFF) {
        data->P3_primary_outlet = pressure_raw / 100.0f;  // 0.01 bar精度
    } else {
        warn(debug_tag, "P3壓力讀取失敗");
        data->P3_primary_outlet = 0.0f;
    }

    // 讀取 P4 二次側進水壓力（控制目標）
    pressure_raw = modbus_read_input_register(REG_P4_PRESSURE);
    if (pressure_raw >= 0 && pressure_raw != 0xFFFF) {
        data->P4_secondary_inlet = pressure_raw / 100.0f;  // 0.01 bar精度
    } else {
        error(debug_tag, "P4壓力讀取失敗 - 這是主要控制目標！");
        data->P4_secondary_inlet = 0.0f;
    }

    // 計算壓力差 (P2 - P4)
    data->pressure_differential = data->P2_secondary_outlet - data->P4_secondary_inlet;
    

    // 設定時間戳
    data->timestamp = time(NULL);

    debug(debug_tag, "壓力數據 - P1: %.2f, P2: %.2f, P3: %.2f, P4: %.2f bar, 壓差(P4-P2): %.2f bar",
          data->P1_primary_inlet, data->P2_secondary_outlet,
          data->P3_primary_outlet, data->P4_secondary_inlet,
          data->pressure_differential);

    return 0;
}

/**
 * PID 控制器計算（參照 ls80_3.c）
 */
static float calculate_pressure_pid_output(pressure_pid_controller_t *pid, float setpoint, float current_value) {
    time_t current_time = time(NULL);
    float delta_time = (current_time > pid->previous_time) ? (current_time - pid->previous_time) : 1.0f;

    // 計算控制誤差
    float error = setpoint - current_value;

    // 比例項
    float proportional = pid->kp * error;

    // 積分項 - 防止積分飽和（簡化邏輯，參照 ls80_3）
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
    //float output = proportional + integral_term + derivative_term;
    float output = proportional + derivative_term;

    // 輸出限制
    if (output > pid->output_max) output = pid->output_max;
    if (output < pid->output_min) output = pid->output_min;

    // 更新狀態
    pid->previous_error = error;
    pid->previous_time = current_time;

    debug(debug_tag, "壓差PID25 - 誤差: %.2f, P: %.2f, I: %.2f, D: %.2f, 輸出: %.2f%%",
          error, proportional, integral_term, derivative_term, output);

    return output;
}

/**
 * 重置 PID 控制器
 */
static void reset_pressure_pid_controller(pressure_pid_controller_t *pid) {
    pid->integral = 0.0f;
    pid->previous_error = 0.0f;
    pid->previous_time = time(NULL);
    debug(debug_tag, "壓差PID控制器已重置");
}

/**
 * 計算泵浦控制策略（完全參照 ls80_3.c 的 calculate_basic_pump_control）
 */
static void calculate_pump_control(float pid_output, pump_control_output_t *output) {
    // PID輸出範圍應為 [-100, +100]，映射到泵速控制
    float abs_pid_output = fabs(pid_output);

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
 * 執行泵浦控制輸出（參照 ls80_3.c）
 */
static void execute_pump_control_output(const pump_control_output_t *output) {
    int pump_registers[2][2] = {
        {REG_PUMP1_SPEED, REG_PUMP1_CONTROL},
        {REG_PUMP2_SPEED, REG_PUMP2_CONTROL}
    };

    // 控制2個泵浦
    for (int i = 0; i < 2; i++) {
        if (output->active_pumps[i]) {
            // 啟動並設定速度（0-100對應0-100%，直接寫入）
            int speed_value = (int)(output->pump_speeds[i]);
            if (speed_value > 100) speed_value = 100;
            if (speed_value < 0) speed_value = 0;

            modbus_write_single_register(pump_registers[i][0], speed_value);
            modbus_write_single_register(pump_registers[i][1], 1);

            debug(debug_tag, "Pump%d 啟動 - 速度: %d%%", i+1, speed_value);
        } else {
            // 停止泵浦
            modbus_write_single_register(pump_registers[i][0], 0);
            modbus_write_single_register(pump_registers[i][1], 0);
            debug(debug_tag, "Pump%d 停止", i+1);
        }
    }
}

/**
 * 手動壓差控制模式
 */
static int execute_manual_pressure_control(float target_pressure_diff) {
    info(debug_tag, "手動壓差控制模式 - 目標壓差: %.2f bar", target_pressure_diff);

    // 手動模式下僅監控，不自動調整設備
    // 外部HMI直接寫入 REG_PUMP1_SPEED/REG_PUMP2_SPEED
    debug(debug_tag, "手動模式：等待操作員手動控制泵浦");

    return 0;
}

/**
 * 自動壓差控制模式
 */
static int execute_automatic_pressure_control(const pressure_sensor_data_t *data) {
    float target_pressure_diff;
    float pid_output;
    pump_control_output_t control_output = {{0}, {0}};

    // 更新時間追蹤（參照 ls80_3.c）
    update_primary_pump_auto_time(1, &pump1_auto_tracker,
                                  REG_PUMP1_AUTO_MODE_HOURS,
                                  REG_PUMP1_AUTO_MODE_MINUTES);
    update_primary_pump_auto_time(2, &pump2_auto_tracker,
                                  REG_PUMP2_AUTO_MODE_HOURS,
                                  REG_PUMP2_AUTO_MODE_MINUTES);
    update_display_auto_time();

    // 檢查並執行主泵切換（參照 ls80_3.c）
    check_and_switch_primary_pump();

    info(debug_tag, "自動壓差控制模式執行 ((P4-P2)→Pset追蹤)");

    // 設定自動模式
    //modbus_write_single_register(REG_PUMP1_MANUAL_MODE, 0);
    //modbus_write_single_register(REG_PUMP2_MANUAL_MODE, 0);

    // 讀取目標壓差設定值
    int target_raw = modbus_read_input_register(REG_PRESSURE_SETPOINT);
    if (target_raw >= 0 && target_raw != 0xFFFF) {
        target_pressure_diff = target_raw / 100.0f;  // 0.01 bar精度
    } else {
        target_pressure_diff = 1.0f;  // 預設值
        warn(debug_tag, "讀取目標壓差失敗，使用預設值: %.2f bar", target_pressure_diff);
    }

    // 當前壓差 = P4 - P2
    float current_pressure_diff = data->pressure_differential;
    float pressure_error = target_pressure_diff - current_pressure_diff;

    info(debug_tag, "(P4-P2)→Pset追蹤: 目標=%.2f bar, 當前=%.2f bar, 誤差=%.2f bar",
         target_pressure_diff, current_pressure_diff, pressure_error);

    // PID控制計算
    pid_output = calculate_pressure_pid_output(&pressure_pid, target_pressure_diff, current_pressure_diff);

    // 計算泵浦控制策略
    calculate_pump_control(pid_output, &control_output);

    // 執行控制輸出
    execute_pump_control_output(&control_output);

    info(debug_tag, "自動壓差控制完成 - PID輸出: %.1f%%, 泵浦速度: Pump1=%.1f%%, Pump2=%.1f%%",
         pid_output, control_output.pump_speeds[0], control_output.pump_speeds[1]);

    return 0;
}

/**
 * 累積時間到寄存器（參照 ls80_3.c）
 * @param hour_reg 小時寄存器
 * @param min_reg 分鐘寄存器
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
 * 更新主泵 AUTO 模式時間（參照 ls80_3.c）
 * - 只在該泵為主泵且 AUTO_START_STOP=1 時累計
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
 * 保存顯示時間到文件（參照 ls80_3.c）
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
    char *json_text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_text == NULL) {
        error(debug_tag, "【斷電保持】轉換 JSON 字符串失敗");
        return FAIL;
    }

    // 寫入文件
    FILE *fp = fopen(DISPLAY_TIME_PERSIST_FILE, "w");
    if (fp == NULL) {
        error(debug_tag, "【斷電保持】無法打開文件寫入: %s", DISPLAY_TIME_PERSIST_FILE);
        free(json_text);
        return FAIL;
    }

    size_t len = strlen(json_text);
    size_t written = fwrite(json_text, 1, len, fp);
    free(json_text);

    // 確保數據寫入磁盤
    fflush(fp);
    fsync(fileno(fp));

    bool write_success = (written == len);
    fclose(fp);

    if (!write_success) {
        error(debug_tag, "【斷電保持】寫入文件失敗: %s", DISPLAY_TIME_PERSIST_FILE);
        return FAIL;
    }

    debug(debug_tag, "【斷電保持】顯示時間已保存: %d 小時 %d 分鐘", hours, minutes);
    return SUCCESS;
}

/**
 * 從文件恢復顯示時間（參照 ls80_3.c）
 *
 * 功能:
 * - 系統啟動時讀取上次保存的顯示時間
 * - 寫入到 45046/45047 寄存器
 *
 * @return SUCCESS 成功, FAIL 失敗
 */
static int restore_display_time_from_file(void) {
    long json_len;

    char *json_text = control_logic_read_entire_file(DISPLAY_TIME_PERSIST_FILE, &json_len);
    if (json_text == NULL) {
        info(debug_tag, "【斷電保持】顯示時間持久化文件不存在,使用預設值 0");
        modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_HOURS, 0);
        modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_MINUTES, 0);
        return FAIL;
    }

    // 解析 JSON
    cJSON *root = cJSON_Parse(json_text);
    free(json_text);

    if (root == NULL) {
        error(debug_tag, "【斷電保持】JSON 解析失敗,刪除損壞文件");
        remove(DISPLAY_TIME_PERSIST_FILE);  // 刪除損壞文件
        return FAIL;
    }

    cJSON *hours_obj = cJSON_GetObjectItem(root, "display_hours");
    cJSON *minutes_obj = cJSON_GetObjectItem(root, "display_minutes");

    if (hours_obj == NULL || minutes_obj == NULL) {
        error(debug_tag, "【斷電保持】JSON 缺少必要字段,刪除損壞文件");
        cJSON_Delete(root);
        remove(DISPLAY_TIME_PERSIST_FILE);  // 刪除損壞文件
        return FAIL;
    }

    uint16_t hours = (uint16_t)hours_obj->valueint;
    uint16_t minutes = (uint16_t)minutes_obj->valueint;

    cJSON_Delete(root);

    // 寫入寄存器
    modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_HOURS, hours);
    modbus_write_single_register(REG_CURRENT_PRIMARY_AUTO_MINUTES, minutes);

    info(debug_tag, "【斷電保持】成功恢復顯示時間: %d 小時 %d 分鐘", hours, minutes);
    return SUCCESS;
}

/**
 * 更新顯示時間（參照 ls80_3.c）
 * 功能:
 * - 偵測主泵變化,變化時自動歸零顯示時間
 * - 在 AUTO 模式下累積時間到顯示時間寄存器 (45046/45047)
 * - 定期保存顯示時間到文件 (每 5 分鐘)
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
 * 檢查並執行主泵切換邏輯（參照 ls80_3.c）
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
 * CDU 壓力差控制主要函數
 *
 * 【函數功能】
 * 這是壓力差控制邏輯的主入口函數，由控制邏輯管理器週期性調用。
 * 實現 (P4-P2)→Pset 壓力差追蹤控制
 *
 * 【執行流程】
 * 1. 檢查控制邏輯是否啟用 (REG_CONTROL_LOGIC_2_ENABLE)
 * 2. 讀取壓力感測器數據 (P1, P2, P3, P4)
 * 3. 計算壓力差 (P4 - P2)
 * 4. 檢查控制模式（手動/自動）
 * 5. 執行對應的控制邏輯
 *
 * @param ptr 控制邏輯結構指標 (本函數未使用)
 * @return 0=成功, -1=感測器讀取失敗, 其他=控制執行失敗
 */
int control_logic_ls80_2_pressure_control(ControlLogic *ptr) {
    (void)ptr;

    // 【步驟0】處理 AUTO_START_STOP 與 FLOW_MODE 聯動控制
    handle_auto_start_stop_and_flow_mode();

    // 【步驟1】檢查控制邏輯2是否啟用，並偵測 1→0 轉換
    uint16_t current_enable = modbus_read_input_register(REG_CONTROL_LOGIC_2_ENABLE);

    // 偵測到 0→1 轉換：保存當前 PUMP_MANUAL_MODE
    if (previous_control_logic2_enable == 0 && current_enable == 1) {
        saved_pump1_manual_mode = modbus_read_input_register(REG_PUMP1_MANUAL_MODE);
        saved_pump2_manual_mode = modbus_read_input_register(REG_PUMP2_MANUAL_MODE);
        info(debug_tag, "【ENABLE_2 0→1】保存 PUMP_MANUAL_MODE - P1=%d, P2=%d",
             saved_pump1_manual_mode, saved_pump2_manual_mode);
    }

    // 偵測到 1→0 轉換：切換到手動模式並保存最後速度
    if (previous_control_logic2_enable == 1 && current_enable == 0) {
        // 保存 PUMP_MANUAL_MODE 狀態 (避免重複保存)
        if (saved_pump1_manual_mode == 0xFFFF) {
            saved_pump1_manual_mode = modbus_read_input_register(REG_PUMP1_MANUAL_MODE);
            saved_pump2_manual_mode = modbus_read_input_register(REG_PUMP2_MANUAL_MODE);
            info(debug_tag, "【ENABLE_2 1→0】保存 PUMP_MANUAL_MODE - P1=%d, P2=%d",
                 saved_pump1_manual_mode, saved_pump2_manual_mode);
        }

        // 執行切換到手動模式
        switch_to_manual_mode_with_last_speed();
    }

    // 更新前一次狀態供下次比較
    previous_control_logic2_enable = current_enable;

    // 若未啟用則退出
    if (current_enable != 1) {
        return 0;  // 未啟用則直接返回
    }

    pressure_sensor_data_t sensor_data;
    int ret = 0;

    info(debug_tag, "=== CDU壓力差控制系統執行 (v01) ===");

    // 【步驟2】讀取壓力感測器數據
    if (read_pressure_sensor_data(&sensor_data) != 0) {
        error(debug_tag, "讀取壓力感測器數據失敗");
        return -1;
    }

    // 【步驟2.5】壓力安全檢查 (不受 control_logic_ls80_2_enable 影響)
    // 注意: 這個檢查在所有模式下都會執行,確保系統安全
    if (check_pressure_limits(&sensor_data) != 0) {
        warn(debug_tag, "壓力超出安全限制範圍,請檢查系統!");
        // 注意: 這裡只記錄警告,不強制停機,實際停機邏輯需要由上層決定
        // 可以根據需求在這裡添加緊急停機邏輯
    }

    // 【步驟3】監控顯示 P1 和 P3
    info(debug_tag, "監控壓力 - P1(一次側進水): %.2f bar, P3(一次側出水): %.2f bar",
         sensor_data.P1_primary_inlet, sensor_data.P3_primary_outlet);

    // 【步驟4】檢查控制模式 (基於 AUTO_START_STOP)
    // 修改理由: 使用 AUTO_START_STOP 判斷模式,避免與需求(保持 PUMP_MANUAL_MODE 不變)的邏輯矛盾
    // AUTO_START_STOP = 1 → 自動模式 (執行 PID 控制)
    // AUTO_START_STOP = 0 → 手動模式 (僅監控)
    uint16_t auto_start_stop = modbus_read_input_register(REG_AUTO_START_STOP);
    int control_mode = (auto_start_stop == 0) ?
                       PRESSURE_CONTROL_MODE_MANUAL : PRESSURE_CONTROL_MODE_AUTO;

    info(debug_tag, "控制模式判斷 - AUTO_START_STOP=%d, control_mode=%s",
         auto_start_stop, (control_mode == PRESSURE_CONTROL_MODE_AUTO) ? "AUTO" : "MANUAL");

    // 讀取目標壓差
    int target_raw = modbus_read_input_register(REG_PRESSURE_SETPOINT);
    float target_pressure_diff = (target_raw >= 0 && target_raw != 0xFFFF) ?
                                 target_raw / 10.0f : 1.0f;

    // 【步驟5】根據模式執行控制
    if (control_mode == PRESSURE_CONTROL_MODE_AUTO) {
        // 自動模式
        info(debug_tag, "執行自動壓差控制模式 ((P2-P4)→Pset追蹤)");
        ret = execute_automatic_pressure_control(&sensor_data);
    } else {
        // 手動模式
        info(debug_tag, "手動壓差控制模式 - 僅監控狀態");
        ret = execute_manual_pressure_control(target_pressure_diff);
    }

    if (ret != 0) {
        error(debug_tag, "壓差控制邏輯執行失敗: %d", ret);
    }

    // 恢復 PUMP_MANUAL_MODE 狀態 (如果在 FLOW_MODE 切換時有保存)
    restore_pump_manual_mode_if_saved();

    debug(debug_tag, "=== CDU壓力差控制循環完成 ===");
    return ret;
}

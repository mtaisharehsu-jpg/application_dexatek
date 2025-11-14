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
static uint32_t REG_P1_PRESSURE = 42082;  // P1一次側進水壓力（監控）
static uint32_t REG_P2_PRESSURE = 42083;  // P2二次側出水壓力（控制）
static uint32_t REG_P3_PRESSURE = 42084;  // P3一次側出水壓力（監控）
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

// ========== 控制參數 ==========
#define PUMP_MIN_SPEED            10.0f   // 泵浦最小速度 %
#define PUMP_MAX_SPEED            100.0f  // 泵浦最大速度 %
#define CONTROL_DEADZONE          3.0f    // 控制死區 %

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
static int read_pressure_sensor_data(pressure_sensor_data_t *data);
static float calculate_pressure_pid_output(pressure_pid_controller_t *pid, float setpoint, float current_value);
static void reset_pressure_pid_controller(pressure_pid_controller_t *pid);
static int execute_manual_pressure_control(float target_pressure_diff);
static int execute_automatic_pressure_control(const pressure_sensor_data_t *data);
static void calculate_pump_control(float pid_output, pump_control_output_t *output);
static void execute_pump_control_output(const pump_control_output_t *output);
static void handle_auto_start_stop_and_flow_mode(void);
static void restore_pump_manual_mode_if_saved(void);

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

    _register_list_init();

    return ret;
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
 * 計算泵浦控制策略（參照 ls80_3.c）
 */
static void calculate_pump_control(float pid_output, pump_control_output_t *output) {
    // 取絕對值處理負誤差（參照 ls80_3）
    float abs_pid_output = fabs(pid_output);

    // 初始化輸出
    for (int i = 0; i < 2; i++) {
        output->active_pumps[i] = 0;
        output->pump_speeds[i] = 0.0f;
    }

    // 死區處理：小於死區時停止所有泵浦
    if (abs_pid_output < CONTROL_DEADZONE) {
        debug(debug_tag, "PID輸出在死區內(%.1f%%)，停止所有泵浦", pid_output);
        return;  // 停止所有泵浦
    }

    // 簡化策略：兩個泵同速運行
    float pump_speed = abs_pid_output;

    // 限制速度範圍
    if (pump_speed > PUMP_MAX_SPEED) pump_speed = PUMP_MAX_SPEED;
    if (pump_speed < PUMP_MIN_SPEED) pump_speed = PUMP_MIN_SPEED;

    // 兩泵同時運行
    output->active_pumps[0] = 1;
    output->active_pumps[1] = 1;
    output->pump_speeds[0] = pump_speed;
    output->pump_speeds[1] = pump_speed;

    debug(debug_tag, "泵浦控制 - PID: %.1f%%, Pump1: %.1f%%, Pump2: %.1f%%",
          pid_output, output->pump_speeds[0], output->pump_speeds[1]);
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

    // 【步驟3】監控顯示 P1 和 P3
    info(debug_tag, "監控壓力 - P1(一次側進水): %.2f bar, P3(一次側出水): %.2f bar",
         sensor_data.P1_primary_inlet, sensor_data.P3_primary_outlet);

    // 【步驟4】檢查控制模式
    int pump1_manual = modbus_read_input_register(REG_PUMP1_MANUAL_MODE);
    int pump2_manual = modbus_read_input_register(REG_PUMP2_MANUAL_MODE);

    int control_mode = (pump1_manual > 0 || pump2_manual > 0) ?
                       PRESSURE_CONTROL_MODE_MANUAL : PRESSURE_CONTROL_MODE_AUTO;

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

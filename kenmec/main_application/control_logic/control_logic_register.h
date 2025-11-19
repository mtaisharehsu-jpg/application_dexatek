/**
 * @file control_logic_register.h
 * @brief 控制邏輯暫存器定義頭文件
 *
 * 本文件定義了控制邏輯系統中所有暫存器的字串標識符和資料結構。
 * 包含以下主要功能：
 * - 控制邏輯啟用/停用暫存器
 * - 溫度感測器暫存器 (T2, T4, T11, T12, T17, T18)
 * - 流量計暫存器 (F1-F4)
 * - 壓力感測器暫存器 (P1-P18)
 * - 幫浦控制暫存器 (PUMP1-PUMP3)
 * - 系統設定暫存器 (目標溫度、壓力、流量等)
 * - 閥門控制暫存器
 * - 警報暫存器
 */

#ifndef CONTROL_LOGIC_REGISTER_H
#define CONTROL_LOGIC_REGISTER_H

/* 控制邏輯啟用暫存器 - 用於啟用或停用各個控制邏輯功能 */
#define REG_CONTROL_LOGIC_1_ENABLE_STR "CONTROL_LOGIC_1_ENABLE" // 41001 - 控制邏輯1啟用開關
#define REG_CONTROL_LOGIC_2_ENABLE_STR "CONTROL_LOGIC_2_ENABLE" // 41002 - 控制邏輯2啟用開關
#define REG_CONTROL_LOGIC_3_ENABLE_STR "CONTROL_LOGIC_3_ENABLE" // 41003 - 控制邏輯3啟用開關
#define REG_CONTROL_LOGIC_4_ENABLE_STR "CONTROL_LOGIC_4_ENABLE" // 41004 - 控制邏輯4啟用開關
#define REG_CONTROL_LOGIC_5_ENABLE_STR "CONTROL_LOGIC_5_ENABLE" // 41005 - 控制邏輯5啟用開關
#define REG_CONTROL_LOGIC_6_ENABLE_STR "CONTROL_LOGIC_6_ENABLE" // 41006 - 控制邏輯6啟用開關
#define REG_CONTROL_LOGIC_7_ENABLE_STR "CONTROL_LOGIC_7_ENABLE" // 41007 - 控制邏輯7啟用開關

/* 系統狀態暫存器 */
#define REG_SYSTEM_STATUS_STR "SYSTEM_STATUS" // 41007 - 系統整體運行狀態

/* 環境監測暫存器 - 環境溫濕度及露點計算 */
#define REG_ENV_TEMP_STR "ENV_TEMP"           // 42021 - 環境溫度（°C）
#define REG_HUMIDITY_STR "HUMIDITY"           // 42022 - 環境濕度（%RH）
#define REG_DEW_POINT_STR "DEW_POINT"         // 42024 - 露點溫度計算值（°C）
#define REG_DP_CORRECT_STR "DP_CORRECT"       // 45004 - 露點校正值（°C）

/* 溫度感測器暫存器 - 讀取各個溫度測點的數值 */
#define REG_T1_TEMP_STR "T1"   // 溫度感測器T1讀值
#define REG_T2_TEMP_STR "T2"   // 溫度感測器T2讀值
#define REG_T3_TEMP_STR "T3"   // 溫度感測器T3讀值
#define REG_T4_TEMP_STR "T4"   // 溫度感測器T4讀值
#define REG_T5_TEMP_STR "T5"   // 溫度感測器T5讀值
#define REG_T6_TEMP_STR "T6"   // 溫度感測器T6讀值
#define REG_T11_TEMP_STR "T11" // 414554 - 溫度感測器T11讀值
#define REG_T12_TEMP_STR "T12" // 414556 - 溫度感測器T12讀值
#define REG_T17_TEMP_STR "T17" // 414566 - 溫度感測器T17讀值
#define REG_T18_TEMP_STR "T18" // 414568 - 溫度感測器T18讀值

#define REG_SYSTEM_STATUS_STR "SYSTEM_STATUS" // 42001 - 系統狀態（重複定義）

/* 流量計暫存器 - 讀取各個流量計的流量數值 */
#define REG_F1_FLOW_STR "F1" // 42062 - 流量計F1讀值
#define REG_F2_FLOW_STR "F2" // 42063 - 流量計F2讀值
#define REG_F3_FLOW_STR "F3" // 42064 - 流量計F3讀值
#define REG_F4_FLOW_STR "F4" // 42065 - 流量計F4讀值

/* 壓力感測器暫存器 - 讀取各個壓力測點的壓力數值 */
#define REG_P1_PRESSURE_STR "P1"   // 42082 - 壓力感測器P1讀值
#define REG_P4_PRESSURE_STR "P4"   // 42085 - 壓力感測器P4讀值
#define REG_P2_PRESSURE_STR "P2"   // 42083 - 壓力感測器P2讀值
#define REG_P3_PRESSURE_STR "P3"   // 42084 - 壓力感測器P3讀值
#define REG_P5_PRESSURE_STR "P5"   // 42086 - 壓力感測器P5讀值
#define REG_P9_PRESSURE_STR "P9"   // 42090 - 壓力感測器P9讀值
#define REG_P11_PRESSURE_STR "P11" // 42092 - 壓力感測器P11讀值
#define REG_P12_PRESSURE_STR "P12" // 42093 - 壓力感測器P12讀值
#define REG_P13_PRESSURE_STR "P13" // 42094 - 壓力感測器P13讀值
#define REG_P15_PRESSURE_STR "P15" // 42096 - 壓力感測器P15讀值
#define REG_P16_PRESSURE_STR "P16" // 42097 - 壓力感測器P16讀值
#define REG_P17_PRESSURE_STR "P17" // 42098 - 壓力感測器P17讀值
#define REG_P18_PRESSURE_STR "P18" // 42099 - 壓力感測器P18讀值

/* 幫浦運行參數暫存器 - 讀取幫浦的頻率、電流、電壓等運行數據 */
#define REG_PUMP1_FREQ_STR "PUMP1_FREQ"       // 42501 - 幫浦1運行頻率（Hz）
#define REG_PUMP2_FREQ_STR "PUMP2_FREQ"       // 42511 - 幫浦2運行頻率（Hz）
#define REG_PUMP3_FREQ_STR "PUMP3_FREQ"       // 42521 - 幫浦3運行頻率（Hz）
#define REG_PUMP1_CURRENT_STR "PUMP1_CURRENT" // 42502 - 幫浦1運行電流（A）
#define REG_PUMP2_CURRENT_STR "PUMP2_CURRENT" // 42512 - 幫浦2運行電流（A）
#define REG_PUMP3_CURRENT_STR "PUMP3_CURRENT" // 42522 - 幫浦3運行電流（A）
#define REG_PUMP1_VOLTAGE_STR "PUMP1_VOLTAGE" // 42503 - 幫浦1運行電壓（V）
#define REG_PUMP2_VOLTAGE_STR "PUMP2_VOLTAGE" // 42513 - 幫浦2運行電壓（V）
#define REG_PUMP3_VOLTAGE_STR "PUMP3_VOLTAGE" // 42523 - 幫浦3運行電壓（V）

/* 系統設定值暫存器 - 設定控制系統的目標參數 */
#define REG_TARGET_TEMP_STR "TARGET_TEMP"               // 45001 - 目標溫度設定值（°C）
#define REG_PRESSURE_SETPOINT_STR "PRESSURE_SETPOINT"   // 45002 - 壓力設定值（bar 或 psi）
#define REG_FLOW_SETPOINT_STR "FLOW_SETPOINT"           // 45003 - 流量設定值（L/min）
#define REG_TARGET_PRESSURE_STR "TARGET_PRESSURE"       // 45004 - 目標壓力設定值

/* 流量控制模式暫存器 */
#define REG_FLOW_MODE_STR "FLOW_MODE"                   // 45005 - 流量控制模式選擇
#define REG_PRESSUR_MODE_STR "PRESSUR_MODE" 

/* 流量限制暫存器 - 設定流量的上下限制範圍 */
#define REG_FLOW_HIGH_LIMIT_STR "FLOW_HIGH_LIMIT"       // 45006 - 流量上限值（L/min）
#define REG_FLOW_LOW_LIMIT_STR "FLOW_LOW_LIMIT"         // 45007 - 流量下限值（L/min）

/* 控制模式暫存器 */
#define REG_TEMP_CONTROL_MODE_STR "TEMP_CONTROL_MODE"   // 45009 - 溫度控制模式（自動/手動）
#define REG_AUTO_START_STOP_STR "AUTO_START_STOP"       // 45020 - 自動啟停功能開關
#define REG_TEMP_FOLLOW_DEW_POINT_STR "TEMP_FOLLOW_DEW_POINT" // 45010 - 溫度跟隨露點模式開關（0=保護模式, 1=跟隨模式）

/* 幫浦手動控制模式暫存器 */
#define REG_PUMP1_MANUAL_MODE_STR "PUMP1_MANUAL_MODE" // 45021 - 幫浦1手動模式開關
#define REG_PUMP2_MANUAL_MODE_STR "PUMP2_MANUAL_MODE" // 45022 - 幫浦2手動模式開關
#define REG_PUMP3_MANUAL_MODE_STR "PUMP3_MANUAL_MODE" // 45023 - 幫浦3手動模式開關

/* 幫浦停止控制暫存器 */
#define REG_PUMP1_STOP_STR "PUMP1_STOP" // 45025 - 幫浦1停止命令
#define REG_PUMP2_STOP_STR "PUMP2_STOP" // 45026 - 幫浦2停止命令
#define REG_PUMP3_STOP_STR "PUMP3_STOP" // 45027 - 幫浦3停止命令

/* 幫浦速度限制暫存器 */
#define REG_PUMP_MIN_SPEED_STR "PUMP_MIN_SPEED" // 45031 - 幫浦最低運轉速度（RPM 或 Hz）
#define REG_PUMP_MAX_SPEED_STR "PUMP_MAX_SPEED" // 45032 - 幫浦最高運轉速度（RPM 或 Hz）

/* 主泵輪換控制暫存器 - 雙泵浦定時輪流主控功能 */
#define REG_PUMP_SWITCH_HOUR_STR "PUMP_SWITCH_HOUR"           // 45034 - 主泵切換時數設定 (小時, 0=停用)
#define REG_PRIMARY_PUMP_INDEX_STR "PRIMARY_PUMP_INDEX"       // 45035 - 當前主泵編號 (1=Pump1, 2=Pump2)
#define REG_PUMP1_USE_STR "PUMP1_USE"                         // 45036 - Pump1 啟用開關 (0=停用, 1=啟用)
#define REG_PUMP2_USE_STR "PUMP2_USE"                         // 45037 - Pump2 啟用開關 (0=停用, 1=啟用)

/* AUTO 模式累計時間暫存器 - 記錄主泵在 AUTO 模式的運轉時間 (斷電保持) */
#define REG_PUMP1_AUTO_MODE_HOURS_STR "PUMP1_AUTO_MODE_HOURS"     // 42170 - Pump1 AUTO 模式累計時間 (小時)
#define REG_PUMP2_AUTO_MODE_HOURS_STR "PUMP2_AUTO_MODE_HOURS"     // 42171 - Pump2 AUTO 模式累計時間 (小時)
#define REG_PUMP1_AUTO_MODE_MINUTES_STR "PUMP1_AUTO_MODE_MINUTES" // 42172 - Pump1 AUTO 模式累計時間 (分鐘)
#define REG_PUMP2_AUTO_MODE_MINUTES_STR "PUMP2_AUTO_MODE_MINUTES" // 42173 - Pump2 AUTO 模式累計時間 (分鐘)

/* 系統時序控制暫存器 - 控制系統運行的各種延遲和時間參數 */
#define REG_TARGET_PRESSURE_STR "TARGET_PRESSURE"       // 45051 - 目標壓力值（重複定義）
#define REG_START_DELAY_STR "START_DELAY"               // 45052 - 啟動延遲時間（秒）
#define REG_MAX_RUN_TIME_STR "MAX_RUN_TIME"             // 45053 - 最大運行時間（秒）
#define REG_COMPLETE_DELAY_STR "COMPLETE_DELAY"         // 45054 - 完成延遲時間（秒）
#define REG_WARNING_DELAY_STR "WARNING_DELAY"           // 45055 - 警告延遲時間（秒）
#define REG_MAX_FAIL_COUNT_STR "MAX_FAIL_COUNT"         // 45056 - 最大失敗次數
#define REG_CURRENT_FAIL_COUNT_STR "CURRENT_FAIL_COUNT" // 42801 - 當前失敗次數

/* 幫浦速度指令暫存器 */
#define REG_PUMP1_SPEED_STR "PUMP1_SPEED" // 411037 - 幫浦1速度指令
#define REG_PUMP2_SPEED_STR "PUMP2_SPEED" // 411039 - 幫浦2速度指令
#define REG_PUMP3_SPEED_STR "PUMP3_SPEED" // 411041 - 幫浦3速度指令

/* 幫浦控制暫存器 */
#define REG_PUMP1_CONTROL_STR "PUMP1_CONTROL" // 411101 DO_0 - 幫浦1控制輸出
#define REG_PUMP2_CONTROL_STR "PUMP2_CONTROL" // 411102 - 幫浦2控制輸出
#define REG_PUMP3_CONTROL_STR "PUMP3_CONTROL" // 411105 - 幫浦3控制輸出

/* 閥門控制暫存器 */
#define REG_VALVE_SETPOINT_STR "VALVE_SETPOINT" // 40046 - 閥門開度設定值（%）
#define REG_VALVE_ACTUAL_STR "VALVE_ACTUAL"     // 40047 - 閥門實際開度回饋值（%）

#define REG_PRESSURE_SETPOINT_STR "PRESSURE_SETPOINT" // 45002 - 壓力設定值（重複定義）

/* 閥門手動模式暫存器 */
#define REG_VALVE_MANUAL_MODE_STR "VALVE_MANUAL_MODE" // 45061 - 閥門手動模式開關

/* 壓力警報暫存器 */
#define REG_HIGH_PRESSURE_ALARM_STR "HIGH_PRESSURE_ALARM"       // 46271 - 高壓警報觸發閾值
#define REG_HIGH_PRESSURE_SHUTDOWN_STR "HIGH_PRESSURE_SHUTDOWN" // 46272 - 高壓關機觸發閾值

/* 溫度限制暫存器 */
#define REG_T_HIGH_ALARM_STR "T_HIGH_ALARM"  // 46001 - 最高溫度限制（°C）
#define REG_T_LOW_ALARM_STR "T_LOW_ALARM"    // 46002 - 最低溫度限制（°C）

/* 壓力限制暫存器 (LS80-2) */
#define REG_P_HIGH_ALARM_STR "P_HIGH_ALARM"  // 46201 - 最高壓力限制（Bar）
#define REG_P_LOW_ALARM_STR "P_LOW_ALARM"    // 46202 - 最低壓力限制（Bar）

/* 流量限制暫存器 (LS80-3) */
#define REG_F_HIGH_ALARM_STR "F_HIGH_ALARM"  // 46401 - 最高流量限制（LPM）
#define REG_F_LOW_ALARM_STR "F_LOW_ALARM"    // 46402 - 最低流量限制（LPM）

/* 幫浦速度命令暫存器 */
#define REG_PUMP1_SPEED_CMD_STR "PUMP1_SPEED_CMD" // 411037 - 幫浦1速度命令（重複定義）
#define REG_PUMP2_SPEED_CMD_STR "PUMP2_SPEED_CMD" // 411039 - 幫浦2速度命令（重複定義）
#define REG_PUMP3_SPEED_CMD_STR "PUMP3_SPEED_CMD" // 411041 - 幫浦3速度命令（重複定義）

/* 幫浦運行命令暫存器 */
#define REG_PUMP1_RUN_CMD_STR "PUMP1_RUN_CMD" // 411101 DO_0 - 幫浦1運行命令
#define REG_PUMP2_RUN_CMD_STR "PUMP2_RUN_CMD" // 411103 DO_2 - 幫浦2運行命令
#define REG_PUMP3_RUN_CMD_STR "PUMP3_RUN_CMD" // 411105 DO_4 - 幫浦3運行命令

/* 幫浦重置命令暫存器 */
#define REG_PUMP1_RESET_CMD_STR "PUMP1_RESET_CMD" // 411102 DO_1 - 幫浦1故障重置命令
#define REG_PUMP2_RESET_CMD_STR "PUMP2_RESET_CMD" // 411104 DO_3 - 幫浦2故障重置命令
#define REG_PUMP3_RESET_CMD_STR "PUMP3_RESET_CMD" // 411106 DO_5 - 幫浦3故障重置命令

/* 水泵控制暫存器 */
#define REG_WATER_PUMP_CONTROL_STR "WATER_PUMP_CONTROL" // 411108 - 水泵控制命令

/* 幫浦故障狀態暫存器 */
#define REG_PUMP1_FAULT_STR "PUMP1_FAULT"       // 411109 - 幫浦1故障狀態（0=正常, 1=故障）
#define REG_PUMP2_FAULT_STR "PUMP2_FAULT"       // 411110 - 幫浦2故障狀態（0=正常, 1=故障）
#define REG_PUMP3_FAULT_STR "PUMP3_FAULT"       // 411111 - 幫浦3故障狀態（0=正常, 1=故障）

/* 液位檢測暫存器 */
#define REG_HIGH_LEVEL_STR "HIGH_LEVEL"         // 411112 - 高液位檢測（0=未達, 1=達到）
#define REG_LOW_LEVEL_STR "LOW_LEVEL"           // 411113 - 低液位檢測（0=未達, 1=達到）
#define REG_LEAK_DETECTION_STR "LEAK_DETECTION" // 411114 - 洩漏檢測（0=正常, 1=洩漏）

/**
 * @brief 控制邏輯暫存器類型列舉
 *
 * 定義暫存器的存取權限類型
 */
typedef enum {
    CONTROL_LOGIC_REGISTER_READ = 0,       /* 唯讀暫存器 - 只能讀取，無法寫入 */
    CONTROL_LOGIC_REGISTER_WRITE = 1,      /* 唯寫暫存器 - 只能寫入，無法讀取 */
    CONTROL_LOGIC_REGISTER_READ_WRITE = 2, /* 讀寫暫存器 - 可以讀取和寫入 */
} control_logic_register_type_t;

/**
 * @brief 控制邏輯暫存器結構
 *
 * 描述單一暫存器的完整資訊，包括名稱、位址和存取類型
 */
typedef struct {
    const char *name;                      /* 暫存器名稱字串 */
    uint32_t *address_ptr;                 /* 暫存器位址指標 */
    uint32_t default_address;              /* 暫存器預設位址 */
    control_logic_register_type_t type;    /* 暫存器存取類型（讀/寫/讀寫） */
} control_logic_register_t;

#endif /* CONTROL_LOGIC_REGISTER_H */ 
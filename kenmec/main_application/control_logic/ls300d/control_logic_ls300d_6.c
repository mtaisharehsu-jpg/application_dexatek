/*
 * control_logic_ls300d_6.c - LS80 閥門控制邏輯 (Control Logic 6: Valve Control)
 *
 * 【功能概述】
 * 本模組實現 CDU 系統的閥門控制功能,支援手動/自動模式切換。
 * 自動模式: 直接將命令值傳遞給狀態寄存器
 * 手動模式: 監控 HMI 設定並更新狀態寄存器
 *
 * 【控制目標】
 * - 閥門狀態控制
 * - 閥門開度範圍: 0-100%
 *
 * 【執行器控制】
 * - 閥門狀態輸出 (REG 40047): 閥門實際狀態 (%)
 * - 閥門命令輸入 (REG 411151): 0-100% 開度設定
 * - 手動/自動模式 (REG 45061): 0=自動, 1=手動
 *
 * 【控制模式】
 * - 自動模式 (REG_VALVE_MANUAL=0): 監控 REG_VALVE_COMMAND 變化,有變化時才傳遞給 REG_VALVE_STATE
 * - 手動模式 (REG_VALVE_MANUAL=1): 監控 HMI 對 REG_VALVE_COMMAND 的設定變化,有變化時才寫入 REG_VALVE_STATE
 *
 * 【系統行為】
 * 1. 系統開機完成後: control_logic_6_enable (41006) 預設為 OFF
 * 2. 當偵測到 REG_VALVE_MANUAL_MODE (45061) = 0 時,自動將 control_logic_6_enable (41006) 設為 ON
 * 3. 自動模式時 (REG_VALVE_MANUAL=0): 監控 modbus_register(411151) 變化 → 有變化時才更新 modbus_register(40047)
 * 4. 手動模式時 (REG_VALVE_MANUAL=1): 監控 HMI 設定 modbus_register(411151) 變化 → 有變化時才更新 modbus_register(40047)
 *
 * 作者: [DK]
 * 日期: 2025
 */

#include "dexatek/main_application/include/application_common.h"
#include "kenmec/main_application/control_logic/control_logic_manager.h"

// ========================================================================================
// 閥門控制寄存器定義 (依據 CDU 控制系統 Modbus 寄存器定義表)
// ========================================================================================

static const char* tag = "ls80_6_valve";

#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_ls80_6.json"
#define CONFIG_REGISTER_LIST_SIZE 15
static control_logic_register_t _control_logic_register_list[CONFIG_REGISTER_LIST_SIZE];

// 閥門控制寄存器
static uint32_t REG_CONTROL_LOGIC_6_ENABLE = 41006; // 控制邏輯6啟用

static uint32_t REG_VALVE_STATE = 40047;      // 閥門狀態輸出值 (%)
static uint32_t REG_VALVE_COMMAND = 411151;   // 閥門命令設定值 (%)
static uint32_t REG_VALVE_MANUAL = 45061;     // 閥門手動模式 (0=自動, 1=手動)

// ========================================================================================
// 資料結構定義
// ========================================================================================

// 閥門控制器結構
typedef struct {
    bool manual_mode;              // 手動模式標誌
    uint16_t last_command_value;   // 上次命令值 (用於檢測 HMI 變化)
    bool system_initialized;       // 系統初始化標誌
    uint32_t cycle_count;          // 執行週期計數
} valve_controller_t;

// ========================================================================================
// 全域變數
// ========================================================================================

static valve_controller_t _valve_controller = {0};

// ========================================================================================
// Modbus 通信函數
// ========================================================================================

// 讀取 Holding Register
static uint16_t read_holding_register(uint32_t address) {
    uint16_t value = 0;
    int ret = control_logic_read_holding_register(address, &value);

    if (ret != SUCCESS) {
        value = 0xFFFF; // 錯誤標記
    }

    return value;
}

// 寫入 Holding Register
static bool write_holding_register(uint32_t address, uint16_t value) {
    int ret = control_logic_write_register(address, value, 2000);
    return (ret == SUCCESS) ? true : false;
}

// ========================================================================================
// 控制邏輯實現
// ========================================================================================

// 自動模式控制: 檢測命令值變化並更新狀態寄存器
static void execute_auto_mode(valve_controller_t* controller) {
    // 讀取命令寄存器
    uint16_t command_value = read_holding_register(REG_VALVE_COMMAND);

    if (command_value == 0xFFFF) {
        error(tag, "自動模式: 讀取 REG_VALVE_COMMAND 失敗");
        return;
    }

    // 檢測命令值是否改變
    if (command_value != controller->last_command_value) {
        // 命令值有變化,將新值寫入狀態寄存器
        if (write_holding_register(REG_VALVE_STATE, command_value)) {
            info(tag, "自動模式: 命令值變化檢測 %u → %u, 已更新 REG_VALVE_STATE",
                 controller->last_command_value, command_value);
            controller->last_command_value = command_value;
        } else {
            error(tag, "自動模式: 寫入 REG_VALVE_STATE 失敗");
        }
    } else {
        // 命令值未變化
        debug(tag, "自動模式: 命令值未變化 (%u)", command_value);
    }
}

// 手動模式控制: 檢測 HMI 設定變化並更新狀態寄存器
static void execute_manual_mode(valve_controller_t* controller) {
    // 讀取當前命令值
    uint16_t current_command = read_holding_register(REG_VALVE_COMMAND);

    if (current_command == 0xFFFF) {
        error(tag, "手動模式: 讀取 REG_VALVE_COMMAND 失敗");
        return;
    }

    // 檢測命令值是否被 HMI 改變
    if (current_command != controller->last_command_value) {
        // HMI 設定了新值,將其寫入狀態寄存器
        if (write_holding_register(REG_VALVE_STATE, current_command)) {
            info(tag, "手動模式: HMI 設定變化檢測 %u → %u, 已更新 REG_VALVE_STATE",
                 controller->last_command_value, current_command);
            controller->last_command_value = current_command;
        } else {
            error(tag, "手動模式: 寫入 REG_VALVE_STATE 失敗");
        }
    } else {
        // 命令值未變化
        debug(tag, "手動模式: 命令值未變化 (%u)", current_command);
    }
}

// ========================================================================================
// 寄存器配置初始化
// ========================================================================================

// 寄存器列表初始化
static int _register_list_init(void)
{
    int ret = SUCCESS;

    _control_logic_register_list[0].name = REG_CONTROL_LOGIC_6_ENABLE_STR;
    _control_logic_register_list[0].address_ptr = &REG_CONTROL_LOGIC_6_ENABLE;
    _control_logic_register_list[0].default_address = REG_CONTROL_LOGIC_6_ENABLE;
    _control_logic_register_list[0].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[1].name = REG_VALVE_ACTUAL_STR;
    _control_logic_register_list[1].address_ptr = &REG_VALVE_STATE;
    _control_logic_register_list[1].default_address = REG_VALVE_STATE;
    _control_logic_register_list[1].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[2].name = REG_VALVE_SETPOINT_STR;
    _control_logic_register_list[2].address_ptr = &REG_VALVE_COMMAND;
    _control_logic_register_list[2].default_address = REG_VALVE_COMMAND;
    _control_logic_register_list[2].type = CONTROL_LOGIC_REGISTER_WRITE;

    _control_logic_register_list[3].name = REG_VALVE_MANUAL_MODE_STR;
    _control_logic_register_list[3].address_ptr = &REG_VALVE_MANUAL;
    _control_logic_register_list[3].default_address = REG_VALVE_MANUAL;
    _control_logic_register_list[3].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // 嘗試從檔案載入寄存器配置
    uint32_t list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    ret = control_logic_register_load_from_file(CONFIG_REGISTER_FILE_PATH, _control_logic_register_list, list_size);
    debug(tag, "從檔案 %s 載入寄存器配置, 結果 %d", CONFIG_REGISTER_FILE_PATH, ret);

    return ret;
}

// 取得寄存器配置
int control_logic_ls300d_6_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

// ========================================================================================
// 主要函數
// ========================================================================================

// 初始化函數
int control_logic_ls300d_6_valve_control_init(void) {
    info(tag, "初始化閥門控制器");

    // 初始化寄存器列表
    _register_list_init();

    // 初始化控制器
    memset(&_valve_controller, 0, sizeof(_valve_controller));

    // 設定 REG_VALVE_MANUAL_MODE 為 0 (自動模式)
    if (write_holding_register(REG_VALVE_MANUAL, 0)) {
        info(tag, "REG_VALVE_MANUAL_MODE 已設為 0 (自動模式)");
    } else {
        warn(tag, "設定 REG_VALVE_MANUAL_MODE 失敗,將在主控制迴圈中重試");
    }

    // 讀取初始命令值
    uint16_t initial_command = read_holding_register(REG_VALVE_COMMAND);
    if (initial_command != 0xFFFF) {
        _valve_controller.last_command_value = initial_command;
        info(tag, "初始命令值: %u", initial_command);
    }

    _valve_controller.system_initialized = true;

    info(tag, "閥門控制器初始化完成");
    return 0;
}

// 主控制函數 - 整合到 control_logic_X 框架
int control_logic_ls300d_6_valve_control(ControlLogic *ptr) {
    if (ptr == NULL) {
        return -1;
    }

    // 讀取當前模式
    uint16_t manual_mode_reg = read_holding_register(REG_VALVE_MANUAL);
    if (manual_mode_reg == 0xFFFF) {
        error(tag, "讀取 REG_VALVE_MANUAL 失敗");
        return -1;
    }

    // 當偵測到 REG_VALVE_MANUAL_MODE = 0 時,將 control_logic_6_enable 設定為 ON
    if (manual_mode_reg == 0) {
        uint16_t current_enable = read_holding_register(REG_CONTROL_LOGIC_6_ENABLE);
        if (current_enable != 1) {
            if (write_holding_register(REG_CONTROL_LOGIC_6_ENABLE, 1)) {
                info(tag, "偵測到 REG_VALVE_MANUAL_MODE = 0, 已將 control_logic_6_enable 設為 ON");
            } else {
                error(tag, "設定 control_logic_6_enable 失敗");
            }
        }
    }

    // 檢查啟用狀態
    uint16_t enable_status = read_holding_register(REG_CONTROL_LOGIC_6_ENABLE);
    if (enable_status != 1) {
        debug(tag, "控制邏輯未啟用 (enable=%u)", enable_status);
        return 0;
    }

    _valve_controller.manual_mode = (manual_mode_reg != 0);

    // 根據模式執行對應控制邏輯
    if (_valve_controller.manual_mode) {
        // 手動模式: 監控 HMI 設定變化
        execute_manual_mode(&_valve_controller);
    } else {
        // 自動模式: 直接傳遞命令值
        execute_auto_mode(&_valve_controller);
    }

    // 更新執行計數
    _valve_controller.cycle_count++;

    return 0;
}

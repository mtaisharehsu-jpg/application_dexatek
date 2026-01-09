/*
 * control_logic_ls300d_7.c - LS80 雙DC泵手動控制邏輯 (Control Logic 7: Dual DC Pump Manual Control)
 *
 * 【功能概述】
 * 本模組實現 CDU 系統的雙 DC 泵浦手動控制功能，專注於簡單的手動模式運作。
 * 系統開機後自動啟動兩個泵浦，並在手動模式下定期更新泵浦速度設定值。
 *
 * 【控制目標】
 * - 系統開機完成後：control_logic_7_enable ON
 * - Pump1 (MS 11001) 和 Pump2 (MS 11002) 自動啟動
 * - 手動模式下每 1.5 秒更新泵浦速度設定值
 *
 * 【感測器配置】
 * - DC泵1回饋: 頻率(REG 42501), 電流(REG 42553), 電壓(REG 42552)
 * - DC泵2回饋: 頻率(REG 42511), 電流(REG 42563), 電壓(REG 42562)
 *
 * 【執行器控制】
 * - DC泵1: 速度(REG 45015, 0-100%), 啟停(REG 411101)
 * - DC泵2: 速度(REG 45016, 0-100%), 啟停(REG 411102)
 *
 * 【控制模式】
 * - 手動模式: 當 PUMP1_MANUAL_MODE_REG (45021) = 1 時
 *   - 若 REG_PUMP1_SPEED (45015) 被從 HMI 設定
 *   - 每 1.5 秒送一次 PUMP1 手動設定值給 REG_PUMP1_SPEED
 * - 手動模式: 當 PUMP2_MANUAL_MODE_REG (45022) = 1 時
 *   - 若 REG_PUMP2_SPEED (45016) 被從 HMI 設定
 *   - 每 1.5 秒送一次 PUMP2 手動設定值給 REG_PUMP2_SPEED
 *
 * 作者: [DK]
 * 日期: 2025
 */

#include "dexatek/main_application/include/application_common.h"
#include "kenmec/main_application/control_logic/control_logic_manager.h"

static const char* debug_tag = "ls300d_7_2dc_pump";

#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_ls300d_7.json"
#define CONFIG_REGISTER_LIST_SIZE 40  // 擴大以容納運轉時間寄存器
static control_logic_register_t _control_logic_register_list[CONFIG_REGISTER_LIST_SIZE];

/*---------------------------------------------------------------------------
                            Register Definitions
 ---------------------------------------------------------------------------*/

static uint32_t REG_CONTROL_LOGIC_7_ENABLE = 41007; // 控制邏輯7啟用

// DC泵浦控制寄存器 (只有2台泵浦)
static uint32_t DC_PUMP1_SPEED_CMD_REG = 45015;   // DC泵1轉速設定
static uint32_t DC_PUMP1_ENABLE_CMD_REG = 411001;   // DC泵1啟停控制
static uint32_t DC_PUMP1_RESET_CMD_REG = 411108;   // DC泵1異常復歸
static uint32_t DC_PUMP1_STATUS_REG = 411109;   // DC泵1過載狀態
static uint32_t DC_PUMP1_FREQ_FB_REG = 42501;    // DC泵1輸出頻率
static uint32_t DC_PUMP1_CURRENT_FB_REG = 42553;    // DC泵1電流
static uint32_t DC_PUMP1_VOLTAGE_FB_REG = 42552;    // DC泵1電壓

static uint32_t DC_PUMP2_SPEED_CMD_REG = 45016;   // DC泵2轉速設定
static uint32_t DC_PUMP2_ENABLE_CMD_REG = 411002;   // DC泵2啟停控制
static uint32_t DC_PUMP2_RESET_CMD_REG = 411110;   // DC泵2異常復歸
static uint32_t DC_PUMP2_STATUS_REG = 411111;   // DC泵2過載狀態
static uint32_t DC_PUMP2_FREQ_FB_REG = 42511;    // DC泵2輸出頻率
static uint32_t DC_PUMP2_CURRENT_FB_REG = 42563;    // DC泵2電流
static uint32_t DC_PUMP2_VOLTAGE_FB_REG = 42562;    // DC泵2電壓

// 控制模式寄存器
static uint32_t AUTO_START_STOP = 45020;    // 系統啟停
static uint32_t PUMP1_MANUAL_MODE_REG = 45021;    // 泵1手動模式
static uint32_t PUMP2_MANUAL_MODE_REG = 45022;    // 泵2手動模式
static uint32_t PUMP1_STOP_REG = 45026;    // 泵1停用
static uint32_t PUMP2_STOP_REG = 45027;    // 泵2停用
static uint32_t PUMP_MIN_SPEED_REG = 45031;    // 泵最小速度
static uint32_t PUMP_MAX_SPEED_REG = 45032;    // 泵最大速度

static uint32_t TARGET_FLOW_REG = 45003;    // 目標流量設定
static uint32_t TARGET_PRESSURE_REG = 45004;    // 目標壓力設定
static uint32_t FLOW_FEEDBACK_REG = 42063;    // 流量回饋
static uint32_t PRESSURE_FEEDBACK_REG = 42093;    // 壓力回饋

// ==================== Pump Runtime Registers ====================
// 泵浦運轉時間記錄寄存器 (斷電保持)
static uint32_t PUMP1_RUNTIME_SEC_REG = 42161;   // Pump1 運轉時間 - 秒 (0-59)
static uint32_t PUMP1_RUNTIME_MIN_REG = 42162;   // Pump1 運轉時間 - 分 (0-59)
static uint32_t PUMP1_RUNTIME_HOUR_REG = 42163;  // Pump1 運轉時間 - 時 (0-23)
static uint32_t PUMP1_RUNTIME_DAY_REG = 42164;   // Pump1 運轉時間 - 天 (累積)

static uint32_t PUMP2_RUNTIME_SEC_REG = 42165;   // Pump2 運轉時間 - 秒 (0-59)
static uint32_t PUMP2_RUNTIME_MIN_REG = 42166;   // Pump2 運轉時間 - 分 (0-59)
static uint32_t PUMP2_RUNTIME_HOUR_REG = 42167;  // Pump2 運轉時間 - 時 (0-23)
static uint32_t PUMP2_RUNTIME_DAY_REG = 42168;   // Pump2 運轉時間 - 天 (累積)

// 運轉時間歸零控制寄存器
static uint32_t PUMP1_RUNTIME_RESET_REG = 45041; // Pump1 運轉時間歸零 (避免與 REG_PUMP2_USE 衝突)
static uint32_t PUMP2_RUNTIME_RESET_REG = 45042; // Pump2 運轉時間歸零 (避免地址衝突)

/*---------------------------------------------------------------------------
                            Static Variables
 ---------------------------------------------------------------------------*/

// 手動模式速度更新計時器 (1.5 秒)
#define MANUAL_SPEED_UPDATE_INTERVAL 1.0  // 秒

// 系統啟動延遲時間（秒）
#define SYSTEM_STARTUP_DELAY 5  // 系統啟動後延遲5秒再啟動泵浦

static time_t pump1_last_update_time = 0;
static time_t pump2_last_update_time = 0;
static uint16_t pump1_last_speed = 0;
static uint16_t pump2_last_speed = 0;
static bool system_initialized = false;
static uint16_t previous_auto_start_stop = 0;  // 用於追蹤 AUTO_START_STOP 的前次狀態

// 泵浦啟動狀態追蹤
static bool pumps_auto_started = false;
static bool pump1_started = false;
static bool pump2_started = false;
static int pump1_retry_count = 0;
static int pump2_retry_count = 0;
static time_t system_start_time = 0;

#define MAX_PUMP_START_RETRY 3  // 最大重試次數

/*---------------------------------------------------------------------------
                        Pump Runtime Tracking Variables
 ---------------------------------------------------------------------------*/

// 泵浦運轉時間追蹤結構
typedef struct {
    time_t last_update_time;    // 上次更新時間戳
    bool last_running_state;    // 上次運轉狀態
    bool initialized;           // 是否已初始化
} pump_runtime_tracker_t;

static pump_runtime_tracker_t pump1_runtime_tracker = {0, false, false};
static pump_runtime_tracker_t pump2_runtime_tracker = {0, false, false};

/*---------------------------------------------------------------------------
                            Modbus Helper Functions
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
    return (ret == SUCCESS) ? true : false;
}

/*---------------------------------------------------------------------------
                            Register List Initialization
 ---------------------------------------------------------------------------*/

static int _register_list_init(void)
{
    int ret = SUCCESS;

    _control_logic_register_list[0].name = REG_CONTROL_LOGIC_7_ENABLE_STR;
    _control_logic_register_list[0].address_ptr = &REG_CONTROL_LOGIC_7_ENABLE;
    _control_logic_register_list[0].default_address = REG_CONTROL_LOGIC_7_ENABLE;
    _control_logic_register_list[0].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[1].name = REG_PUMP1_SPEED_STR;
    _control_logic_register_list[1].address_ptr = &DC_PUMP1_SPEED_CMD_REG;
    _control_logic_register_list[1].default_address = DC_PUMP1_SPEED_CMD_REG;
    _control_logic_register_list[1].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[2].name = REG_PUMP1_CONTROL_STR;
    _control_logic_register_list[2].address_ptr = &DC_PUMP1_ENABLE_CMD_REG;
    _control_logic_register_list[2].default_address = DC_PUMP1_ENABLE_CMD_REG;
    _control_logic_register_list[2].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[3].name = REG_PUMP1_RESET_CMD_STR;
    _control_logic_register_list[3].address_ptr = &DC_PUMP1_RESET_CMD_REG;
    _control_logic_register_list[3].default_address = DC_PUMP1_RESET_CMD_REG;
    _control_logic_register_list[3].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[4].name = REG_PUMP1_FAULT_STR;
    _control_logic_register_list[4].address_ptr = &DC_PUMP1_STATUS_REG;
    _control_logic_register_list[4].default_address = DC_PUMP1_STATUS_REG;
    _control_logic_register_list[4].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[5].name = REG_PUMP1_FREQ_STR;
    _control_logic_register_list[5].address_ptr = &DC_PUMP1_FREQ_FB_REG;
    _control_logic_register_list[5].default_address = DC_PUMP1_FREQ_FB_REG;
    _control_logic_register_list[5].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[6].name = REG_PUMP1_CURRENT_STR;
    _control_logic_register_list[6].address_ptr = &DC_PUMP1_CURRENT_FB_REG;
    _control_logic_register_list[6].default_address = DC_PUMP1_CURRENT_FB_REG;
    _control_logic_register_list[6].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[7].name = REG_PUMP1_VOLTAGE_STR;
    _control_logic_register_list[7].address_ptr = &DC_PUMP1_VOLTAGE_FB_REG;
    _control_logic_register_list[7].default_address = DC_PUMP1_VOLTAGE_FB_REG;
    _control_logic_register_list[7].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[8].name = REG_PUMP2_SPEED_STR;
    _control_logic_register_list[8].address_ptr = &DC_PUMP2_SPEED_CMD_REG;
    _control_logic_register_list[8].default_address = DC_PUMP2_SPEED_CMD_REG;
    _control_logic_register_list[8].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[9].name = REG_PUMP2_CONTROL_STR;
    _control_logic_register_list[9].address_ptr = &DC_PUMP2_ENABLE_CMD_REG;
    _control_logic_register_list[9].default_address = DC_PUMP2_ENABLE_CMD_REG;
    _control_logic_register_list[9].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[10].name = REG_PUMP2_RESET_CMD_STR;
    _control_logic_register_list[10].address_ptr = &DC_PUMP2_RESET_CMD_REG;
    _control_logic_register_list[10].default_address = DC_PUMP2_RESET_CMD_REG;
    _control_logic_register_list[10].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[11].name = REG_PUMP2_FAULT_STR;
    _control_logic_register_list[11].address_ptr = &DC_PUMP2_STATUS_REG;
    _control_logic_register_list[11].default_address = DC_PUMP2_STATUS_REG;
    _control_logic_register_list[11].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[12].name = REG_PUMP2_FREQ_STR;
    _control_logic_register_list[12].address_ptr = &DC_PUMP2_FREQ_FB_REG;
    _control_logic_register_list[12].default_address = DC_PUMP2_FREQ_FB_REG;
    _control_logic_register_list[12].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[13].name = REG_PUMP2_CURRENT_STR;
    _control_logic_register_list[13].address_ptr = &DC_PUMP2_CURRENT_FB_REG;
    _control_logic_register_list[13].default_address = DC_PUMP2_CURRENT_FB_REG;
    _control_logic_register_list[13].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[14].name = REG_PUMP2_VOLTAGE_STR;
    _control_logic_register_list[14].address_ptr = &DC_PUMP2_VOLTAGE_FB_REG;
    _control_logic_register_list[14].default_address = DC_PUMP2_VOLTAGE_FB_REG;
    _control_logic_register_list[14].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[15].name = REG_AUTO_START_STOP_STR;
    _control_logic_register_list[15].address_ptr = &AUTO_START_STOP;
    _control_logic_register_list[15].default_address = AUTO_START_STOP;
    _control_logic_register_list[15].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[16].name = REG_PUMP1_MANUAL_MODE_STR;
    _control_logic_register_list[16].address_ptr = &PUMP1_MANUAL_MODE_REG;
    _control_logic_register_list[16].default_address = PUMP1_MANUAL_MODE_REG;
    _control_logic_register_list[16].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[17].name = REG_PUMP2_MANUAL_MODE_STR;
    _control_logic_register_list[17].address_ptr = &PUMP2_MANUAL_MODE_REG;
    _control_logic_register_list[17].default_address = PUMP2_MANUAL_MODE_REG;
    _control_logic_register_list[17].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[18].name = REG_PUMP1_STOP_STR;
    _control_logic_register_list[18].address_ptr = &PUMP1_STOP_REG;
    _control_logic_register_list[18].default_address = PUMP1_STOP_REG;
    _control_logic_register_list[18].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[19].name = REG_PUMP2_STOP_STR;
    _control_logic_register_list[19].address_ptr = &PUMP2_STOP_REG;
    _control_logic_register_list[19].default_address = PUMP2_STOP_REG;
    _control_logic_register_list[19].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[20].name = REG_PUMP_MIN_SPEED_STR;
    _control_logic_register_list[20].address_ptr = &PUMP_MIN_SPEED_REG;
    _control_logic_register_list[20].default_address = PUMP_MIN_SPEED_REG;
    _control_logic_register_list[20].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[21].name = REG_PUMP_MAX_SPEED_STR;
    _control_logic_register_list[21].address_ptr = &PUMP_MAX_SPEED_REG;
    _control_logic_register_list[21].default_address = PUMP_MAX_SPEED_REG;
    _control_logic_register_list[21].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[22].name = REG_FLOW_SETPOINT_STR;
    _control_logic_register_list[22].address_ptr = &TARGET_FLOW_REG;
    _control_logic_register_list[22].default_address = TARGET_FLOW_REG;
    _control_logic_register_list[22].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[23].name = REG_TARGET_PRESSURE_STR;
    _control_logic_register_list[23].address_ptr = &TARGET_PRESSURE_REG;
    _control_logic_register_list[23].default_address = TARGET_PRESSURE_REG;
    _control_logic_register_list[23].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[24].name = REG_F2_FLOW_STR;
    _control_logic_register_list[24].address_ptr = &FLOW_FEEDBACK_REG;
    _control_logic_register_list[24].default_address = FLOW_FEEDBACK_REG;
    _control_logic_register_list[24].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[25].name = REG_P12_PRESSURE_STR;
    _control_logic_register_list[25].address_ptr = &PRESSURE_FEEDBACK_REG;
    _control_logic_register_list[25].default_address = PRESSURE_FEEDBACK_REG;
    _control_logic_register_list[25].type = CONTROL_LOGIC_REGISTER_READ;

    // 泵浦1運轉時間寄存器
    _control_logic_register_list[26].name = REG_PUMP1_RUNTIME_SEC_STR;
    _control_logic_register_list[26].address_ptr = &PUMP1_RUNTIME_SEC_REG;
    _control_logic_register_list[26].default_address = PUMP1_RUNTIME_SEC_REG;
    _control_logic_register_list[26].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[27].name = REG_PUMP1_RUNTIME_MIN_STR;
    _control_logic_register_list[27].address_ptr = &PUMP1_RUNTIME_MIN_REG;
    _control_logic_register_list[27].default_address = PUMP1_RUNTIME_MIN_REG;
    _control_logic_register_list[27].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[28].name = REG_PUMP1_RUNTIME_HOUR_STR;
    _control_logic_register_list[28].address_ptr = &PUMP1_RUNTIME_HOUR_REG;
    _control_logic_register_list[28].default_address = PUMP1_RUNTIME_HOUR_REG;
    _control_logic_register_list[28].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[29].name = REG_PUMP1_RUNTIME_DAY_STR;
    _control_logic_register_list[29].address_ptr = &PUMP1_RUNTIME_DAY_REG;
    _control_logic_register_list[29].default_address = PUMP1_RUNTIME_DAY_REG;
    _control_logic_register_list[29].type = CONTROL_LOGIC_REGISTER_READ;

    // 泵浦2運轉時間寄存器
    _control_logic_register_list[30].name = REG_PUMP2_RUNTIME_SEC_STR;
    _control_logic_register_list[30].address_ptr = &PUMP2_RUNTIME_SEC_REG;
    _control_logic_register_list[30].default_address = PUMP2_RUNTIME_SEC_REG;
    _control_logic_register_list[30].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[31].name = REG_PUMP2_RUNTIME_MIN_STR;
    _control_logic_register_list[31].address_ptr = &PUMP2_RUNTIME_MIN_REG;
    _control_logic_register_list[31].default_address = PUMP2_RUNTIME_MIN_REG;
    _control_logic_register_list[31].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[32].name = REG_PUMP2_RUNTIME_HOUR_STR;
    _control_logic_register_list[32].address_ptr = &PUMP2_RUNTIME_HOUR_REG;
    _control_logic_register_list[32].default_address = PUMP2_RUNTIME_HOUR_REG;
    _control_logic_register_list[32].type = CONTROL_LOGIC_REGISTER_READ;

    _control_logic_register_list[33].name = REG_PUMP2_RUNTIME_DAY_STR;
    _control_logic_register_list[33].address_ptr = &PUMP2_RUNTIME_DAY_REG;
    _control_logic_register_list[33].default_address = PUMP2_RUNTIME_DAY_REG;
    _control_logic_register_list[33].type = CONTROL_LOGIC_REGISTER_READ;

    // 運轉時間重置寄存器
    _control_logic_register_list[34].name = REG_PUMP1_RUNTIME_RESET_STR;
    _control_logic_register_list[34].address_ptr = &PUMP1_RUNTIME_RESET_REG;
    _control_logic_register_list[34].default_address = PUMP1_RUNTIME_RESET_REG;
    _control_logic_register_list[34].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    _control_logic_register_list[35].name = REG_PUMP2_RUNTIME_RESET_STR;
    _control_logic_register_list[35].address_ptr = &PUMP2_RUNTIME_RESET_REG;
    _control_logic_register_list[35].default_address = PUMP2_RUNTIME_RESET_REG;
    _control_logic_register_list[35].type = CONTROL_LOGIC_REGISTER_READ_WRITE;

    // try to load register array from file
    uint32_t list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    ret = control_logic_register_load_from_file(CONFIG_REGISTER_FILE_PATH, _control_logic_register_list, list_size);
    debug(debug_tag, "load register array from file %s, ret %d", CONFIG_REGISTER_FILE_PATH, ret);

    return ret;
}

int control_logic_ls300d_7_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

/*---------------------------------------------------------------------------
                            Initialization Function
 ---------------------------------------------------------------------------*/

int control_logic_ls300d_7_2dc_pump_control_init(void)
{
    info(debug_tag, "初始化2台DC泵手動控制系統...");

    // register list init
    _register_list_init();

    // 啟用控制邏輯7
    modbus_write_single_register(REG_CONTROL_LOGIC_7_ENABLE, 1);
    info(debug_tag, "控制邏輯7已啟用");

    // 記錄系統啟動時間（延遲啟動泵浦，避免 Modbus 系統未就緒）
    system_start_time = time(NULL);
    pumps_auto_started = false;

    info(debug_tag, "泵浦將在系統穩定後15 %d 秒自動啟動", SYSTEM_STARTUP_DELAY);

    // 初始化時間戳
    pump1_last_update_time = time(NULL);
    pump2_last_update_time = time(NULL);

    // 讀取當前速度值
    pump1_last_speed = modbus_read_input_register(DC_PUMP1_SPEED_CMD_REG);
    pump2_last_speed = modbus_read_input_register(DC_PUMP2_SPEED_CMD_REG);

    system_initialized = true;

    info(debug_tag, "2台DC泵手動控制系統初始化完成");

    return 0;
}

/*---------------------------------------------------------------------------
                            Manual Speed Update Function
 ---------------------------------------------------------------------------*/

/**
 * 手動模式速度更新函數 - Pump1
 *
 * 【執行條件】
 * - 當 AUTO_START_STOP (45020) = 0 時執行
 *
 * 【功能】
 * - 每 1.0 秒更新一次速度設定值
 * - 允許從 HMI 手動設定 Pump1 速度 (REG 45015)
 */
static void update_pump1_manual_speed(void)
{
    // 檢查條件：只要 AUTO_START_STOP = 0 就允許手動速度更新
    uint16_t auto_start_stop = modbus_read_input_register(AUTO_START_STOP);
    if (auto_start_stop != 0) {
        return; // AUTO_START_STOP != 0，不執行手動速度更新
    }

    // 讀取當前速度設定值
    uint16_t current_speed = modbus_read_input_register(DC_PUMP1_SPEED_CMD_REG);

    // 檢查是否從 HMI 設定了新值（速度有變化）
    bool speed_changed = (current_speed != pump1_last_speed);

    // 檢查是否已經過了 1.5 秒
    time_t current_time = time(NULL);
    double elapsed_time = difftime(current_time, pump1_last_update_time);

    if (speed_changed || elapsed_time >= MANUAL_SPEED_UPDATE_INTERVAL) {
        // 重新寫入速度值
        modbus_write_single_register(DC_PUMP1_SPEED_CMD_REG, current_speed);

        // 更新記錄
        pump1_last_speed = current_speed;
        pump1_last_update_time = current_time;

        debug(debug_tag, "Pump1 手動速度更新: %d%% (間隔: %.1f秒)",
              current_speed, elapsed_time);
    }
}

/**
 * 手動模式速度更新函數 - Pump2
 *
 * 【執行條件】
 * - 當 AUTO_START_STOP (45020) = 0 時執行
 *
 * 【功能】
 * - 每 1.0 秒更新一次速度設定值
 * - 允許從 HMI 手動設定 Pump2 速度 (REG 45016)
 */
static void update_pump2_manual_speed(void)
{
    // 檢查條件：只要 AUTO_START_STOP = 0 就允許手動速度更新
    uint16_t auto_start_stop = modbus_read_input_register(AUTO_START_STOP);
    if (auto_start_stop != 0) {
        return; // AUTO_START_STOP != 0，不執行手動速度更新
    }

    // 讀取當前速度設定值
    uint16_t current_speed = modbus_read_input_register(DC_PUMP2_SPEED_CMD_REG);

    // 檢查是否從 HMI 設定了新值（速度有變化）
    bool speed_changed = (current_speed != pump2_last_speed);

    // 檢查是否已經過了 1.5 秒
    time_t current_time = time(NULL);
    double elapsed_time = difftime(current_time, pump2_last_update_time);

    if (speed_changed || elapsed_time >= MANUAL_SPEED_UPDATE_INTERVAL) {
        // 重新寫入速度值
        modbus_write_single_register(DC_PUMP2_SPEED_CMD_REG, current_speed);

        // 更新記錄
        pump2_last_speed = current_speed;
        pump2_last_update_time = current_time;

        debug(debug_tag, "Pump2 手動速度更新: %d%% (間隔: %.1f秒)",
              current_speed, elapsed_time);
    }
}

/*---------------------------------------------------------------------------
                            Auto-Start Pumps Function
 ---------------------------------------------------------------------------*/

/**
 * 延遲啟動泵浦函數
 *
 * 系統啟動後延遲指定秒數才啟動泵浦，確保 Modbus 系統已完全就緒
 * 啟動後會讀取實際狀態確認，失敗會自動重試
 */
static void auto_start_pumps_delayed(void)
{
    // 如果已經啟動過，直接返回
    if (pumps_auto_started) {
        return;
    }

    // 檢查是否已經過了延遲時間
    time_t current_time = time(NULL);
    double elapsed_time = difftime(current_time, system_start_time);

    if (elapsed_time < SYSTEM_STARTUP_DELAY) {
        // 還沒到啟動時間
        debug(debug_tag, "等待系統穩定中... (%.1f / %d 秒)",
              elapsed_time, SYSTEM_STARTUP_DELAY);
        return;
    }

    // 時間到了，開始啟動泵浦
    if (!pump1_started && !pump2_started) {
        info(debug_tag, "系統已穩定 %.1f 秒，開始自動啟動泵浦...", elapsed_time);
    }

    // ========== 啟動 Pump1 (MS 11001) ==========
    if (!pump1_started) {
        if (pump1_retry_count < MAX_PUMP_START_RETRY) {
            if (pump1_retry_count > 0) {
                info(debug_tag, "重試啟動 Pump1 (MS 11001)... (第 %d 次重試)",
                     pump1_retry_count);
            } else {
                info(debug_tag, "嘗試啟動 Pump1 (MS 11001)...");
            }

            // 寫入啟動命令
            bool write_result = modbus_write_single_register(DC_PUMP1_ENABLE_CMD_REG, 1);
            if (!write_result) {
                error(debug_tag, "✗ Pump1 寫入命令失敗");
                pump1_retry_count++;
                return;
            }

            // 等待設備反應
            usleep(500000); // 0.5 秒

            // 讀取實際狀態確認
            uint16_t pump1_status = modbus_read_input_register(DC_PUMP1_ENABLE_CMD_REG);
            if (pump1_status == 1) {
                info(debug_tag, "✓ Pump1 狀態確認: %d (已啟動)", pump1_status);
                pump1_started = true;
            } else {
                error(debug_tag, "✗ Pump1 狀態確認失敗: 讀到 %d，預期 1 (重試 %d/%d)",
                      pump1_status, pump1_retry_count + 1, MAX_PUMP_START_RETRY);
                pump1_retry_count++;
                return; // 確認失敗，下次重試
            }
        } else {
            error(debug_tag, "✗✗ Pump1 啟動失敗，已達最大重試次數 %d", MAX_PUMP_START_RETRY);
            pump1_started = true; // 標記為已處理，不再重試
        }
    }

    // ========== 啟動 Pump2 (MS 11002) ==========
    if (!pump2_started) {
        if (pump2_retry_count < MAX_PUMP_START_RETRY) {
            if (pump2_retry_count > 0) {
                info(debug_tag, "重試啟動 Pump2 (MS 11002)... (第 %d 次重試)",
                     pump2_retry_count);
            } else {
                info(debug_tag, "嘗試啟動 Pump2 (MS 11002)...");
            }

            // 寫入啟動命令
            bool write_result = modbus_write_single_register(DC_PUMP2_ENABLE_CMD_REG, 1);
            if (!write_result) {
                error(debug_tag, "✗ Pump2 寫入命令失敗");
                pump2_retry_count++;
                return;
            }

            // 等待設備反應
            usleep(500000); // 0.5 秒

            // 讀取實際狀態確認
            uint16_t pump2_status = modbus_read_input_register(DC_PUMP2_ENABLE_CMD_REG);
            if (pump2_status == 1) {
                info(debug_tag, "✓ Pump2 狀態確認: %d (已啟動)", pump2_status);
                pump2_started = true;
            } else {
                error(debug_tag, "✗ Pump2 狀態確認失敗: 讀到 %d，預期 1 (重試 %d/%d)",
                      pump2_status, pump2_retry_count + 1, MAX_PUMP_START_RETRY);
                pump2_retry_count++;
                return; // 確認失敗，下次重試
            }
        } else {
            error(debug_tag, "✗✗ Pump2 啟動失敗，已達最大重試次數 %d", MAX_PUMP_START_RETRY);
            pump2_started = true; // 標記為已處理，不再重試
        }
    }

    // ========== 檢查兩個泵浦是否都已處理完成 ==========
    if (pump1_started && pump2_started) {
        pumps_auto_started = true;

        // 再次讀取確認最終狀態
        uint16_t final_pump1 = modbus_read_input_register(DC_PUMP1_ENABLE_CMD_REG);
        uint16_t final_pump2 = modbus_read_input_register(DC_PUMP2_ENABLE_CMD_REG);

        if (final_pump1 == 1 && final_pump2 == 1) {
            info(debug_tag, "✓✓ 兩個泵浦已全部成功啟動並確認 (Pump1=%d, Pump2=%d)",
                 final_pump1, final_pump2);
        } else {
            warn(debug_tag, "⚠ 泵浦啟動程序完成，但狀態異常 (Pump1=%d, Pump2=%d)",
                 final_pump1, final_pump2);
        }
    }
}

/*---------------------------------------------------------------------------
                    AUTO_START_STOP Edge Trigger Handler
 ---------------------------------------------------------------------------*/

/**
 * 處理 AUTO_START_STOP 寄存器的邊緣觸發
 *
 * 當 AUTO_START_STOP (45020) 從 1 變為 0 時，自動將兩個泵浦速度降到 0
 *
 * 【功能說明】
 * - 檢測 AUTO_START_STOP 的 1→0 邊緣觸發
 * - 自動設定 DC_PUMP1_SPEED_CMD_REG (45015) = 0
 * - 自動設定 DC_PUMP2_SPEED_CMD_REG (45016) = 0
 * - 不修改 PUMP1_MANUAL_MODE_REG 和 PUMP2_MANUAL_MODE_REG 的原值
 *
 * 【使用範例】
 * 當使用者將 AUTO_START_STOP 從 1 切換到 0 時，系統會自動將兩個泵浦
 * 速度降到 0，並允許後續從 HMI 手動設定速度（只要 AUTO_START_STOP = 0）。
 */
static void handle_auto_start_stop(void) {
    // 讀取自動啟停開關 (45020)
    uint16_t current_auto_start = modbus_read_input_register(AUTO_START_STOP);

    // 檢查讀取是否成功
    if (current_auto_start == 0xFFFF) {
        warn(debug_tag, "AUTO_START_STOP 讀取失敗，跳過邊緣觸發檢查");
        return;  // 讀取失敗，跳過處理
    }

    // 邊緣觸發檢測：從 1 變為 0 時，將泵浦速度降到 0
    if (previous_auto_start_stop == 1 && current_auto_start == 0) {
        info(debug_tag, "【自動啟停】關閉 (1→0) - 將兩個泵浦速度降到 0");

        // 設定 Pump1 速度為 0
        bool pump1_success = modbus_write_single_register(DC_PUMP1_SPEED_CMD_REG, 0);
        if (!pump1_success) {
            error(debug_tag, "【自動啟停】設定 Pump1 速度為 0 失敗");
        }

        // 設定 Pump2 速度為 0
        bool pump2_success = modbus_write_single_register(DC_PUMP2_SPEED_CMD_REG, 0);
        if (!pump2_success) {
            error(debug_tag, "【自動啟停】設定 Pump2 速度為 0 失敗");
        }

        // 記錄執行結果
        if (pump1_success && pump2_success) {
            info(debug_tag, "【自動啟停】成功將泵浦速度降到 0 - Pump1=0%%, Pump2=0%%");
        } else {
            error(debug_tag, "【自動啟停】降速部分失敗 - Pump1=%s, Pump2=%s",
                  pump1_success ? "成功" : "失敗",
                  pump2_success ? "成功" : "失敗");
        }
    }

    // 更新前次狀態
    previous_auto_start_stop = current_auto_start;
}

/*---------------------------------------------------------------------------
                       Pump Runtime Management Functions
 ---------------------------------------------------------------------------*/

/**
 * 累積泵浦運轉時間 (處理秒→分→時→天的進位)
 *
 * @param sec_reg   秒寄存器地址
 * @param min_reg   分寄存器地址
 * @param hour_reg  時寄存器地址
 * @param day_reg   天寄存器地址
 * @param elapsed   已運轉的秒數
 */
static void accumulate_pump_runtime(uint32_t sec_reg, uint32_t min_reg,
                                   uint32_t hour_reg, uint32_t day_reg,
                                   time_t elapsed) {
    if (elapsed <= 0) return;

    // 讀取當前運轉時間
    uint16_t sec = modbus_read_input_register(sec_reg);
    uint16_t min = modbus_read_input_register(min_reg);
    uint16_t hour = modbus_read_input_register(hour_reg);
    uint16_t day = modbus_read_input_register(day_reg);

    info(debug_tag, "累積前 - 秒:%d 分:%d 時:%d 天:%d (即將累積 %ld 秒) [寄存器: %d/%d/%d/%d]",
          sec, min, hour, day, (long)elapsed, sec_reg, min_reg, hour_reg, day_reg);

    // 累積秒數
    sec += elapsed;

    // 秒進位到分 (60秒 = 1分)
    if (sec >= 60) {
        min += sec / 60;
        sec = sec % 60;
    }

    // 分進位到時 (60分 = 1時)
    if (min >= 60) {
        hour += min / 60;
        min = min % 60;
    }

    // 時進位到天 (24時 = 1天)
    if (hour >= 24) {
        day += hour / 24;
        hour = hour % 24;
    }

    // 寫回寄存器
    bool write_sec = modbus_write_single_register(sec_reg, sec);
    bool write_min = modbus_write_single_register(min_reg, min);
    bool write_hour = modbus_write_single_register(hour_reg, hour);
    bool write_day = modbus_write_single_register(day_reg, day);

    // info(debug_tag, "累積後 - 秒:%d 分:%d 時:%d 天:%d [寫入結果: %d/%d/%d/%d]",
    //      sec, min, hour, day, write_sec, write_min, write_hour, write_day);

    // 特別檢查小時寄存器寫入失敗
    if (!write_hour) {
        error(debug_tag, "【警告】小時寄存器 %d 寫入失敗！值=%d", hour_reg, hour);

        // 重試一次
        bool retry_result = modbus_write_single_register(hour_reg, hour);
        if (retry_result) {
            info(debug_tag, "小時寄存器重試寫入成功");
        } else {
            error(debug_tag, "小時寄存器重試寫入仍然失敗！");
        }
    }
}

/**
 * 檢查並處理運轉時間歸零指令
 *
 * @param reset_reg 歸零控制寄存器地址
 * @param sec_reg   秒寄存器地址
 * @param min_reg   分寄存器地址
 * @param hour_reg  時寄存器地址
 * @param day_reg   天寄存器地址
 * @param pump_name 泵浦名稱 (用於 debug 訊息)
 */
static void check_and_reset_pump_runtime(uint32_t reset_reg, uint32_t sec_reg,
                                        uint32_t min_reg, uint32_t hour_reg,
                                        uint32_t day_reg, const char *pump_name) {
    uint16_t reset_cmd = modbus_read_input_register(reset_reg);

    if (reset_cmd == 1) {
        // 執行歸零
        modbus_write_single_register(sec_reg, 0);
        modbus_write_single_register(min_reg, 0);
        modbus_write_single_register(hour_reg, 0);
        modbus_write_single_register(day_reg, 0);

        // 清除重置命令，避免下次重複觸發
        modbus_write_single_register(reset_reg, 0);

        info(debug_tag, "【%s 運轉時間歸零】執行完成，重置命令已清除", pump_name);
    }
}

/**
 * 更新單個泵浦的運轉時間
 *
 * @param tracker      運轉時間追蹤器
 * @param speed_cmd_reg 泵浦轉速命令寄存器地址
 * @param sec_reg      秒寄存器地址
 * @param min_reg      分寄存器地址
 * @param hour_reg     時寄存器地址
 * @param day_reg      天寄存器地址
 * @param pump_name    泵浦名稱 (用於 debug 訊息)
 */
static void update_pump_runtime(pump_runtime_tracker_t *tracker,
                               uint32_t speed_cmd_reg, uint32_t sec_reg,
                               uint32_t min_reg, uint32_t hour_reg,
                               uint32_t day_reg, const char *pump_name) {
    // 讀取泵浦轉速命令 (> 0 表示運轉中)
    uint16_t speed_cmd = modbus_read_input_register(speed_cmd_reg);
    uint16_t is_running = (speed_cmd > 0) ? 1 : 0;
    time_t current_time = time(NULL);

    debug(debug_tag, "【%s】速度命令=%d%%, 運轉狀態=%d, 寄存器地址=0x%X",
          pump_name, speed_cmd, is_running, speed_cmd_reg);

    // 初始化追蹤器
    if (!tracker->initialized) {
        tracker->last_update_time = current_time;
        tracker->last_running_state = (is_running == 1);
        tracker->initialized = true;
        // info(debug_tag, "【%s 運轉時間追蹤】初始化完成 - 速度=%d%%, 運轉=%d",
        //      pump_name, speed_cmd, is_running);
        return;
    }

    // 如果泵浦正在運轉，累積運轉時間
    if (is_running == 1 && tracker->last_running_state) {
        // 計算經過的時間 (秒)
        time_t elapsed = difftime(current_time, tracker->last_update_time);

        // debug(debug_tag, "【%s】持續運轉中 - 經過時間=%ld秒, 上次狀態=%d",
        //       pump_name, (long)elapsed, tracker->last_running_state);

        if (elapsed >= 1) {  // 至少累積 1 秒
            // info(debug_tag, "【%s 運轉時間】開始累積 %ld 秒 (寄存器: %d/%d/%d/%d)",
            //      pump_name, (long)elapsed, sec_reg, min_reg, hour_reg, day_reg);
            accumulate_pump_runtime(sec_reg, min_reg, hour_reg, day_reg, elapsed);
            tracker->last_update_time = current_time;
            //info(debug_tag, "【%s 運轉時間】累積完成", pump_name);
        }
    } else {
        // 泵浦停止或狀態改變，更新時間戳
        if (tracker->last_running_state != (is_running == 1)) {
            // info(debug_tag, "【%s】運轉狀態改變: %d → %d",
            //      pump_name, tracker->last_running_state, is_running);
        }
        tracker->last_update_time = current_time;
    }

    // 更新運轉狀態
    tracker->last_running_state = (is_running == 1);
}

/**
 * 管理所有泵浦的運轉時間追蹤
 *
 * 【功能說明】
 * 1. 檢查並處理運轉時間歸零指令
 * 2. 更新 Pump1 和 Pump2 的運轉時間累積
 * 3. 每秒累積運轉時間，自動進位 (秒→分→時→天)
 */
static void manage_all_pumps_runtime(void) {
    // 檢查並處理歸零指令
    check_and_reset_pump_runtime(PUMP1_RUNTIME_RESET_REG,
                                 PUMP1_RUNTIME_SEC_REG, PUMP1_RUNTIME_MIN_REG,
                                 PUMP1_RUNTIME_HOUR_REG, PUMP1_RUNTIME_DAY_REG,
                                 "Pump1");

    check_and_reset_pump_runtime(PUMP2_RUNTIME_RESET_REG,
                                 PUMP2_RUNTIME_SEC_REG, PUMP2_RUNTIME_MIN_REG,
                                 PUMP2_RUNTIME_HOUR_REG, PUMP2_RUNTIME_DAY_REG,
                                 "Pump2");

    // 更新運轉時間 (根據轉速命令判斷運轉狀態)
    update_pump_runtime(&pump1_runtime_tracker, DC_PUMP1_SPEED_CMD_REG,
                       PUMP1_RUNTIME_SEC_REG, PUMP1_RUNTIME_MIN_REG,
                       PUMP1_RUNTIME_HOUR_REG, PUMP1_RUNTIME_DAY_REG,
                       "Pump1");

    update_pump_runtime(&pump2_runtime_tracker, DC_PUMP2_SPEED_CMD_REG,
                       PUMP2_RUNTIME_SEC_REG, PUMP2_RUNTIME_MIN_REG,
                       PUMP2_RUNTIME_HOUR_REG, PUMP2_RUNTIME_DAY_REG,
                       "Pump2");
}

/*---------------------------------------------------------------------------
                            Main Control Function
 ---------------------------------------------------------------------------*/

/**
 * 2台DC泵手動控制主要函數
 *
 * 【函數功能】
 * 這是雙 DC 泵浦手動控制邏輯的主入口函數，由控制邏輯管理器週期性調用。
 *
 * 【執行流程】
 * 1. 檢查控制邏輯是否啟用 (REG_CONTROL_LOGIC_7_ENABLE)
 * 2. 延遲自動啟動泵浦（系統啟動後5秒）
 * 3. 處理 AUTO_START_STOP 邊緣觸發（1→0 時自動切換到手動模式）
 * 4. 檢查並更新 Pump1 手動模式速度 (每 1.0 秒)
 * 5. 檢查並更新 Pump2 手動模式速度 (每 1.0 秒)
 *
 * @param ptr 控制邏輯結構指標 (本函數未使用)
 * @return 0=成功
 */
int control_logic_ls300d_7_2dc_pump_control(ControlLogic *ptr) {
    if (ptr == NULL) return -1;

    // 檢查控制邏輯7是否啟用
    if (modbus_read_input_register(REG_CONTROL_LOGIC_7_ENABLE) != 1) {
        return 0;  // 未啟用則直接返回
    }

    debug(debug_tag, "=== 2台DC泵手動控制執行 ===");

    // 延遲自動啟動泵浦（系統啟動後首次執行）
    auto_start_pumps_delayed();

    // 處理 AUTO_START_STOP 邊緣觸發（1→0 時切換到手動模式）
    handle_auto_start_stop();

    // 更新 Pump1 手動模式速度
    update_pump1_manual_speed();

    // 更新 Pump2 手動模式速度
    update_pump2_manual_speed();

    // 管理泵浦運轉時間追蹤
    manage_all_pumps_runtime();

    debug(debug_tag, "=== 2台DC泵手動控制完成 ===");

    return 0;
}

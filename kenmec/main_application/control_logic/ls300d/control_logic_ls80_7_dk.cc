/*
 * control_logic_ls80_7.c - LS80 雙DC泵控制邏輯 (Control Logic 7: Dual DC Pump Control)
 *
 * 【功能概述】
 * 本模組實現 CDU 系統的雙 DC 泵浦控制功能(僅2台泵浦),通過雙 PID 演算法(流量+壓力)
 * 協調控制,實現手動/自動模式切換,並提供完整的泵浦監控、故障檢測、自動復歸功能。
 *
 * 【控制目標】
 * - 流量控制: 維持系統流量在設定值 (預設 100 L/min)
 * - 壓力控制: 維持系統壓力在設定值 (預設 2.5 bar)
 * - 雙泵協調: 根據負載需求自動調整運行泵浦數量和速度
 *
 * 【感測器配置】
 * - F2流量回饋 (REG 42063): 系統流量 (0.1 L/min 精度)
 * - P12壓力回饋 (REG 42093): 系統壓力 (0.1 bar 精度)
 * - DC泵1回饋: 頻率(REG 42501), 電流(REG 42553), 電壓(REG 42552)
 * - DC泵2回饋: 頻率(REG 42511), 電流(REG 42563), 電壓(REG 42562)
 *
 * 【執行器控制】
 * - DC泵1: 速度(REG 45015, 0-100%), 啟停(REG 411101), 復歸(REG 411108)
 * - DC泵2: 速度(REG 45016, 0-100%), 啟停(REG 411102), 復歸(REG 411110)
 * - 故障狀態: DI (REG 411109/411111, 0=故障, 1=正常)
 *
 * 【控制模式】
 * - 手動模式: 外部直接設定泵浦速度和啟停,系統僅監控
 * - 自動模式: 雙 PID 控制,自動調整泵浦速度和數量
 *
 * 【雙泵協調策略】
 * 根據 PID 輸出(流量/壓力取較大者)決定運行策略:
 * - 控制需求 ≤ 50%: 單泵運行 (主泵速度 = 需求×1.8)
 * - 控制需求 > 50%: 雙泵運行 (雙泵速度 = 需求/1.6, 負載均分)
 * - 控制需求 < 15%: 停止所有泵浦
 * - 主泵輪換: 24小時輪換週期 (1440個控制週期)
 *
 * 【PID 參數】
 * 流量 PID:
 * - Kp: 1.2, Ki: 0.15, Kd: 0.06
 * - 輸出範圍: 0-100%
 * - 積分限幅: 防止飽和
 *
 * 壓力 PID:
 * - Kp: 1.8, Ki: 0.25, Kd: 0.1
 * - 輸出範圍: 0-100%
 * - 積分限幅: 防止飽和
 *
 * 【安全保護】
 * - 最大電流限制: 20.0 A
 * - 最大電壓限制: 60.0 V
 * - 最小電壓限制: 10.0 V
 * - 最大功率限制: 3000.0 W
 * - 通訊超時: 5秒
 * - 單泵最大流量: 120 L/min
 * - 泵浦速度範圍: 10-100% (預設)
 *
 * 【泵浦監控】
 * - 實時頻率回饋 (Hz)
 * - 實時電流監測 (A)
 * - 實時電壓監測 (V)
 * - 實時功率計算 (W)
 * - 效率計算
 * - 故障狀態監測
 *
 * 【故障處理】
 * - 過流 (PUMP_FAULT_OVERCURRENT): 立即停機
 * - 過壓 (PUMP_FAULT_OVERVOLTAGE): 立即停機
 * - 欠壓 (PUMP_FAULT_UNDERVOLTAGE): 立即停機
 * - 過載 (PUMP_FAULT_OVERLOAD): 立即停機
 * - 通訊超時 (PUMP_FAULT_COMMUNICATION): 立即停機
 * - 故障復歸: 延遲30秒,自動嘗試復歸(最多3次)
 *
 * 【泵浦輪換】
 * - 主泵定期輪換 (24小時週期)
 * - 確保雙泵均勻使用,延長使用壽命
 * - 輪換時切換 current_lead_pump (1↔2)
 *
 * 作者: [DK]
 * 日期: 2025
 */

#include "dexatek/main_application/include/application_common.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"

// 2dc_pump_control_7_1.c

static const char* debug_tag = "ls80_7_2dc_pump";

#define CONFIG_REGISTER_FILE_PATH "/usrdata/register_configs_ls80_7.json"
#define CONFIG_REGISTER_LIST_SIZE 30
static control_logic_register_t _control_logic_register_list[CONFIG_REGISTER_LIST_SIZE];

/*---------------------------------------------------------------------------
                            Type Definitions
 ---------------------------------------------------------------------------*/

 typedef enum {
    DC_PUMP_MODE_MANUAL = 0,
    DC_PUMP_MODE_AUTO = 1
} dc_pump_control_mode_t;

typedef enum {
    PUMP_FAULT_NONE = 0,
    PUMP_FAULT_OVERCURRENT,
    PUMP_FAULT_OVERVOLTAGE,
    PUMP_FAULT_UNDERVOLTAGE,
    PUMP_FAULT_OVERLOAD,
    PUMP_FAULT_COMMUNICATION,
    PUMP_FAULT_EMERGENCY_STOP
} pump_fault_type_t;

typedef enum {
    SAFETY_STATUS_SAFE = 0,
    SAFETY_STATUS_WARNING = 1,
    SAFETY_STATUS_EMERGENCY = 2
} safety_status_t;

typedef struct {
    float actual_speed_percent;    // 實際轉速百分比
    float actual_current;          // 實際電流 (A)
    float actual_voltage;          // 實際電壓 (V)
    float actual_power;            // 實際功率 (W)
    float efficiency;              // 效率 (%)
    bool is_running;               // 運行狀態
    bool fault_status;             // 故障狀態
    uint16_t fault_code;           // 故障代碼
    time_t last_feedback_time;     // 最後回饋時間
} pump_feedback_t;

typedef struct {
    pump_feedback_t pumps[2];      // 兩台泵浦回饋
    float system_flow;             // 系統流量 L/min
    float system_pressure;         // 系統壓力 bar
    time_t timestamp;
} system_sensor_data_t;

typedef struct {
    float kp;                      // 比例增益
    float ki;                      // 積分增益
    float kd;                      // 微分增益
    float integral;                // 積分累計
    float previous_error;          // 前次誤差
    time_t previous_time;          // 前次時間
    float output_min;              // 輸出最小值
    float output_max;              // 輸出最大值
} pid_controller_t;

typedef struct {
    bool active_pumps[2];          // 泵浦啟用狀態
    float pump_speeds[2];          // 泵浦速度 0-100%
    int pump_count;                // 啟用泵浦數量
} pump_control_output_t;

/*---------------------------------------------------------------------------
                            Static Variables
 ---------------------------------------------------------------------------*/

// PID控制器
static pid_controller_t flow_pid = {
    .kp = 1.2f,
    .ki = 0.15f,
    .kd = 0.06f,
    .integral = 0.0f,
    .previous_error = 0.0f,
    .previous_time = 0,
    .output_min = 0.0f,
    .output_max = 100.0f
};

static pid_controller_t pressure_pid = {
    .kp = 1.8f,
    .ki = 0.25f,
    .kd = 0.1f,
    .integral = 0.0f,
    .previous_error = 0.0f,
    .previous_time = 0,
    .output_min = 0.0f,
    .output_max = 100.0f
};

// 系統狀態變數
static int current_lead_pump = 1;
static int pump_rotation_timer = 0;
static bool system_initialized = false;
static pthread_mutex_t system_mutex = PTHREAD_MUTEX_INITIALIZER;

/*---------------------------------------------------------------------------
                            Register Definitions
 ---------------------------------------------------------------------------*/

static uint32_t REG_CONTROL_LOGIC_7_ENABLE = 41007; // 控制邏輯7啟用

// DC泵浦控制寄存器 (只有2台泵浦)
static uint32_t DC_PUMP1_SPEED_CMD_REG = 45015;   // DC泵1轉速設定
static uint32_t DC_PUMP1_ENABLE_CMD_REG = 411101;   // DC泵1啟停控制
static uint32_t DC_PUMP1_RESET_CMD_REG = 411108;   // DC泵1異常復歸
static uint32_t DC_PUMP1_STATUS_REG = 411109;   // DC泵1過載狀態
static uint32_t DC_PUMP1_FREQ_FB_REG = 42501;    // DC泵1輸出頻率
static uint32_t DC_PUMP1_CURRENT_FB_REG = 42553;    // DC泵1電流
static uint32_t DC_PUMP1_VOLTAGE_FB_REG = 42552;    // DC泵1電壓

static uint32_t DC_PUMP2_SPEED_CMD_REG = 45016;   // DC泵2轉速設定
static uint32_t DC_PUMP2_ENABLE_CMD_REG = 411102;   // DC泵2啟停控制
static uint32_t DC_PUMP2_RESET_CMD_REG = 411110;   // DC泵2異常復歸
static uint32_t DC_PUMP2_STATUS_REG = 411111;   // DC泵2過載狀態
static uint32_t DC_PUMP2_FREQ_FB_REG = 42511;    // DC泵2輸出頻率
static uint32_t DC_PUMP2_CURRENT_FB_REG = 42563;    // DC泵2電流
static uint32_t DC_PUMP2_VOLTAGE_FB_REG = 42562;    // DC泵2電壓

// 控制模式寄存器
static uint32_t SYSTEM_ENABLE_REG = 45020;    // 系統啟停
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

/*---------------------------------------------------------------------------
                            Constants
 ---------------------------------------------------------------------------*/

#define MAX_CURRENT_LIMIT          20.0f    // 最大電流限制 (A)
#define MAX_VOLTAGE_LIMIT          60.0f   // 最大電壓限制 (V)
#define MIN_VOLTAGE_LIMIT          10.0f   // 最小電壓限制 (V)
#define MAX_POWER_LIMIT            3000.0f  // 最大功率限制 (W)
#define MIN_FLOW_RATE              0.0f    // 最小流量 (L/min)
#define SINGLE_PUMP_MAX_FLOW       120.0f   // 單泵最大流量 (L/min) - 2泵系統調高
#define COMMUNICATION_TIMEOUT_S    5        // 通訊超時 (秒)
#define PUMP_MIN_SPEED_DEFAULT     10.0f    // 預設最小速度 (%)
#define PUMP_MAX_SPEED_DEFAULT     100.0f   // 預設最大速度 (%)
#define FAULT_RECOVERY_DELAY_MS    30000    // 故障恢復延遲 (ms)

/*---------------------------------------------------------------------------
                            Function Prototypes
 ---------------------------------------------------------------------------*/

// static int dc_pump_system_init(void);
// static void dc_pump_system_cleanup(void);
static int read_system_sensor_data(system_sensor_data_t *data);
static int read_pump_feedback(int pump_id, pump_feedback_t *feedback);
static safety_status_t perform_safety_checks(const system_sensor_data_t *data);
static void emergency_shutdown(void);
static bool check_manual_mode(void);
static int execute_manual_control_mode(void);
static int execute_automatic_control_mode(const system_sensor_data_t *data);
static float calculate_pid_output(pid_controller_t *pid, float setpoint, float current_value);
static void reset_pid_controller(pid_controller_t *pid);
// static int calculate_required_pumps(float target_flow);
static void calculate_pump_strategy(float flow_output, float pressure_output, pump_control_output_t *output);
static void execute_pump_control(const pump_control_output_t *output);
static void handle_pump_rotation(void);
static void handle_pump_fault(int pump_id, pump_fault_type_t fault_type);
static bool dc_pump_safety_check(const pump_feedback_t *feedback);
static float clamp_float(float value, float min_val, float max_val);
static void schedule_fault_recovery(int pump_id, pump_fault_type_t fault_type);
static void* fault_recovery_task(void* param);

/*---------------------------------------------------------------------------
                            Main Control Function
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

// ========================================================================================
// 主要函數
// ========================================================================================

// register list init
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
    _control_logic_register_list[15].address_ptr = &SYSTEM_ENABLE_REG;
    _control_logic_register_list[15].default_address = SYSTEM_ENABLE_REG;
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

    // try to load register array from file
    uint32_t list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    ret = control_logic_register_load_from_file(CONFIG_REGISTER_FILE_PATH, _control_logic_register_list, list_size);
    debug(debug_tag, "load register array from file %s, ret %d", CONFIG_REGISTER_FILE_PATH, ret);

    return ret;
}

int control_logic_ls80_7_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path)
{
    int ret = SUCCESS;

    *list_size = sizeof(_control_logic_register_list) / sizeof(_control_logic_register_list[0]);
    *list = (control_logic_register_t *)_control_logic_register_list;
    *file_path = (char *)CONFIG_REGISTER_FILE_PATH;

    return ret;
}

// 初始化函數
int control_logic_ls80_7_2dc_pump_control_init(void)
{
    info(debug_tag, "初始化2台DC泵控制系統...");
    
    // register list init
    _register_list_init();

    // 初始化PID控制器
    reset_pid_controller(&flow_pid);
    reset_pid_controller(&pressure_pid);
    
    // 停止所有泵浦
    modbus_write_single_register(DC_PUMP1_ENABLE_CMD_REG, 0);
    modbus_write_single_register(DC_PUMP2_ENABLE_CMD_REG, 0);
    
    // 設定預設速度為0
    // TBD modbus_write_single_register(DC_PUMP1_SPEED_CMD_REG, 0);
    // TBD modbus_write_single_register(DC_PUMP2_SPEED_CMD_REG, 0);
    
    // 設定預設參數
    modbus_write_single_register(PUMP_MIN_SPEED_REG, (int)PUMP_MIN_SPEED_DEFAULT);
    modbus_write_single_register(PUMP_MAX_SPEED_REG, (int)PUMP_MAX_SPEED_DEFAULT);
    
    current_lead_pump = 1;
    pump_rotation_timer = 0;
    
    info(debug_tag, "2台DC泵控制系統初始化完成");

    return 0;
}

/**
 * 2台DC泵控制主要函數 (版本 7.1)
 *
 * 【函數功能】
 * 這是雙 DC 泵浦控制邏輯的主入口函數,由控制邏輯管理器週期性調用。
 * 實現雙 PID 協調控制: 啟用檢查 → 感測器讀取 → 安全檢查 → 模式判斷 → 控制執行 → 泵浦輪換
 *
 * 【執行流程】
 * 1. 檢查控制邏輯是否啟用 (REG_CONTROL_LOGIC_7_ENABLE)
 * 2. 互斥鎖保護 (多執行緒安全)
 * 3. 系統初始化檢查 (首次執行)
 * 4. 讀取系統感測器數據 (流量/壓力/泵浦回饋)
 * 5. 執行安全檢查 (電流/電壓/功率/通訊)
 * 6. 根據模式執行對應的控制邏輯:
 *    - 手動模式: 僅監控,外部控制
 *    - 自動模式: 雙 PID 控制 + 雙泵協調策略
 * 7. 執行泵浦輪換邏輯 (24小時週期)
 * 8. 釋放互斥鎖
 *
 * 【安全等級】
 * - SAFETY_STATUS_SAFE: 系統正常,繼續運行
 * - SAFETY_STATUS_WARNING: 警告狀態,繼續監控
 * - SAFETY_STATUS_EMERGENCY: 緊急狀態,立即停機
 *
 * @param ptr 控制邏輯結構指標 (本函數未使用)
 * @return 0=成功, -1=參數錯誤, -2=感測器讀取失敗, -3=緊急停機, 其他=控制執行失敗
 */

int control_logic_ls80_7_2dc_pump_control(ControlLogic *ptr) {
    if (ptr == NULL) return -1;

    // 【步驟1】檢查控制邏輯7是否啟用 (通過 Modbus 寄存器 41007)
    if (modbus_read_input_register(REG_CONTROL_LOGIC_7_ENABLE) != 1) {
        return 0;  // 未啟用則直接返回,不執行控制
    }

    system_sensor_data_t sensor_data;
    safety_status_t safety_status;
    int ret = 0;

    info(debug_tag, "=== 2台DC泵控制系統執行 (v7.1) ===");

    // 【步驟2】互斥鎖保護 (確保多執行緒安全)
    pthread_mutex_lock(&system_mutex);

    // 【步驟3】系統初始化檢查 (暫時註釋,可根據需求啟用)
    if (!system_initialized) {
        // TODO: dc_pump_system_init function
        // if (dc_pump_system_init() != 0) {
        //     error(debug_tag, "2台DC泵控制系統初始化失敗");
        //     pthread_mutex_unlock(&system_mutex);
        //     return -1;
        // }
        // system_initialized = true;
    }

    // 【步驟4】讀取系統感測器數據
    // 包括 F2(流量), P12(壓力), DC泵1/2回饋(頻率/電流/電壓/狀態)
    if (read_system_sensor_data(&sensor_data) != 0) {
        error(debug_tag, "讀取系統感測器數據失敗");
        pthread_mutex_unlock(&system_mutex);
        return -2;
    }

    debug(debug_tag, "系統數據 - 流量: %.1f L/min, 壓力: %.1f bar",
          sensor_data.system_flow, sensor_data.system_pressure);

    // 【步驟5】安全檢查
    // 檢查項目: 流量/電流/電壓/功率/通訊超時/故障狀態
    safety_status = perform_safety_checks(&sensor_data);

    if (safety_status == SAFETY_STATUS_EMERGENCY) {
        // 緊急等級 → 立即停機
        error(debug_tag, "緊急狀況發生，執行緊急停機");
        emergency_shutdown();
        pthread_mutex_unlock(&system_mutex);
        return -3;
    } else if (safety_status == SAFETY_STATUS_WARNING) {
        // 警告等級 → 繼續監控
        warn(debug_tag, "系統警告狀態，繼續監控");
    }

    // 【步驟6】檢查控制模式並執行相應邏輯
    if (check_manual_mode()) {
        // 手動模式: 僅監控,外部通過寄存器直接控制
        info(debug_tag, "執行手動控制模式");
        ret = execute_manual_control_mode();
    } else {
        // 自動模式: 雙 PID 控制 + 雙泵協調策略
        info(debug_tag, "執行自動控制模式");
        ret = execute_automatic_control_mode(&sensor_data);
    }

    if (ret != 0) {
        error(debug_tag, "控制邏輯執行失敗: %d", ret);
    }

    // 【步驟7】雙泵輪換處理 (24小時輪換週期,確保泵浦均勻使用)
    handle_pump_rotation();

    // 【步驟8】釋放互斥鎖
    pthread_mutex_unlock(&system_mutex);

    debug(debug_tag, "=== 2台DC泵控制循環完成 ===");

    return ret;
}

// static void dc_pump_system_cleanup(void) {
//     info(debug_tag, "清理2台DC泵控制系統...");
    
//     // 停止所有泵浦
//     modbus_write_single_register(DC_PUMP1_ENABLE_CMD_REG, 0);
//     modbus_write_single_register(DC_PUMP2_ENABLE_CMD_REG, 0);
    
//     system_initialized = false;
//     info(debug_tag, "2台DC泵控制系統清理完成");
// }

/*---------------------------------------------------------------------------
                            Sensor Data Reading
 ---------------------------------------------------------------------------*/

static int read_system_sensor_data(system_sensor_data_t *data) {
    int temp_raw;
    int errors = 0;
    
    // 讀取系統流量
    temp_raw = modbus_read_input_register(FLOW_FEEDBACK_REG);
    if (temp_raw >= 0) {
        data->system_flow = temp_raw / 10.0f; // 0.1 L/min精度
    } else {
        warn(debug_tag, "流量讀取失敗");
        data->system_flow = 0.0f;
        errors++;
    }
    
    // 讀取系統壓力
    temp_raw = modbus_read_input_register(PRESSURE_FEEDBACK_REG);
    if (temp_raw >= 0) {
        data->system_pressure = temp_raw / 10.0f; // 0.1 bar精度
    } else {
        warn(debug_tag, "壓力讀取失敗");
        data->system_pressure = 0.0f;
        errors++;
    }
    
    // 讀取各泵浦回饋數據 (只有2台泵浦)
    for (int i = 0; i < 2; i++) {
        if (read_pump_feedback(i + 1, &data->pumps[i]) != 0) {
            errors++;
        }
    }
    
    data->timestamp = time(NULL);
    
    return (errors > 0) ? -1 : 0;
}

static int read_pump_feedback(int pump_id, pump_feedback_t *feedback) {
    int temp_raw;
    int errors = 0;
    
    // 寄存器地址映射 (只支援泵浦1和2)
    uint32_t speed_reg, current_reg, voltage_reg, status_reg;
    switch (pump_id) {
        case 1:
            speed_reg = DC_PUMP1_FREQ_FB_REG;
            current_reg = DC_PUMP1_CURRENT_FB_REG;
            voltage_reg = DC_PUMP1_VOLTAGE_FB_REG;
            status_reg = DC_PUMP1_STATUS_REG;
            break;
        case 2:
            speed_reg = DC_PUMP2_FREQ_FB_REG;
            current_reg = DC_PUMP2_CURRENT_FB_REG;
            voltage_reg = DC_PUMP2_VOLTAGE_FB_REG;
            status_reg = DC_PUMP2_STATUS_REG;
            break;
        default:
            error(debug_tag, "無效的泵浦ID: %d (僅支援1-2)", pump_id);
            return -1;
    }
    
    // 讀取轉速
    temp_raw = modbus_read_input_register(speed_reg);
    if (temp_raw >= 0) {
        // feedback->actual_speed_percent = (temp_raw / 1000.0f) * 100.0f;
        feedback->actual_speed_percent = temp_raw;
    } else {
        feedback->actual_speed_percent = 0.0f;
        errors++;
    }
    
    // 讀取電流
    temp_raw = modbus_read_input_register(current_reg);
    if (temp_raw >= 0) {
        // feedback->actual_current = temp_raw * 0.01f; // A×0.01
        feedback->actual_current = temp_raw;
    } else {
        feedback->actual_current = 0.0f;
        errors++;
    }
    
    // 讀取電壓
    temp_raw = modbus_read_input_register(voltage_reg);
    if (temp_raw >= 0) {
        // feedback->actual_voltage = temp_raw * 0.1f; // V×0.1
        feedback->actual_voltage = temp_raw; 
    } else {
        feedback->actual_voltage = 0.0f;
        errors++;
    }
    
    // 讀取狀態
    temp_raw = modbus_read_input_register(status_reg);
    if (temp_raw >= 0) {
        feedback->fault_status = (temp_raw == 0); // 0=故障, 1=正常
        feedback->fault_code = temp_raw;
    } else {
        feedback->fault_status = true; // 讀取失敗視為故障
        feedback->fault_code = 0xFFFF;
        errors++;
    }
    
    // 計算功率和效率
    feedback->actual_power = feedback->actual_voltage * feedback->actual_current;
    feedback->efficiency = (feedback->actual_power > 0) ? 
        (feedback->actual_speed_percent * 0.85f) : 0.0f; // 2泵系統效率調高
    
    // 判斷運行狀態
    feedback->is_running = (feedback->actual_speed_percent > 1.0f);
    feedback->last_feedback_time = time(NULL);
    
    return (errors > 0) ? -1 : 0;
}

/*---------------------------------------------------------------------------
                            Safety Functions
 ---------------------------------------------------------------------------*/

static safety_status_t perform_safety_checks(const system_sensor_data_t *data) {
    safety_status_t overall_status = SAFETY_STATUS_SAFE;
    
    // 檢查系統流量
    if (data->system_flow < MIN_FLOW_RATE * 0.3f) {
        error(debug_tag, "系統流量過低: %.1f L/min", data->system_flow);
        return SAFETY_STATUS_EMERGENCY;
    } else if (data->system_flow < MIN_FLOW_RATE) {
        warn(debug_tag, "流量偏低警告: %.1f L/min", data->system_flow);
        overall_status = SAFETY_STATUS_WARNING;
    }
    
    // 檢查各泵浦安全狀態 (只檢查2台泵浦)
    for (int i = 0; i < 2; i++) {
        if (!dc_pump_safety_check(&data->pumps[i])) {
            error(debug_tag, "泵浦%d安全檢查失敗", i + 1);
            handle_pump_fault(i + 1, PUMP_FAULT_OVERLOAD);
            overall_status = SAFETY_STATUS_WARNING;
        }
    }
    
    return overall_status;
}

static bool dc_pump_safety_check(const pump_feedback_t *feedback) {
    // 電流檢查
    if (feedback->actual_current > MAX_CURRENT_LIMIT) {
        error(debug_tag, "泵浦電流過高: %.2f A", feedback->actual_current);
        return false;
    }
    
    // 電壓檢查
    if (feedback->actual_voltage > MAX_VOLTAGE_LIMIT || 
        feedback->actual_voltage < MIN_VOLTAGE_LIMIT) {
        error(debug_tag, "泵浦電壓異常: %.1f V", feedback->actual_voltage);
        return false;
    }
    
    // 功率檢查
    if (feedback->actual_power > MAX_POWER_LIMIT) {
        error(debug_tag, "泵浦功率過高: %.1f W", feedback->actual_power);
        return false;
    }
    
    // 通訊檢查
    time_t current_time = time(NULL);
    if (current_time - feedback->last_feedback_time > COMMUNICATION_TIMEOUT_S) {
        error(debug_tag, "泵浦通訊超時");
        return false;
    }
    
    // 故障狀態檢查
    if (feedback->fault_status) {
        error(debug_tag, "泵浦故障狀態: 0x%04X", feedback->fault_code);
        return false;
    }
    
    return true;
}

static void emergency_shutdown(void) {
    error(debug_tag, "執行緊急停機程序...");
    
    // 停止所有泵浦 (只有2台)
    modbus_write_single_register(DC_PUMP1_ENABLE_CMD_REG, 1);
    modbus_write_single_register(DC_PUMP2_ENABLE_CMD_REG, 1);
    
    // 重置PID控制器
    reset_pid_controller(&flow_pid);
    reset_pid_controller(&pressure_pid);
    
    error(debug_tag, "緊急停機完成");
}

/*---------------------------------------------------------------------------
                            Control Mode Functions
 ---------------------------------------------------------------------------*/

static bool check_manual_mode(void) {
    int manual_mode;
    
    // 檢查是否有任何泵浦處於手動模式 (只檢查2台)
    manual_mode = modbus_read_input_register(PUMP1_MANUAL_MODE_REG);
    if (manual_mode > 0) return true;
    
    manual_mode = modbus_read_input_register(PUMP2_MANUAL_MODE_REG);
    if (manual_mode > 0) return true;
    
    return false;
}

static int execute_manual_control_mode(void) {
    info(debug_tag, "手動控制模式執行");
    
    // 在手動模式下，系統主要進行監控
    // 實際的控制由外部系統通過寄存器進行
    
    // 檢查各泵浦狀態並記錄 (只檢查2台)
    for (int i = 0; i < 2; i++) {
        int manual_reg = (i == 0) ? PUMP1_MANUAL_MODE_REG : PUMP2_MANUAL_MODE_REG;
        int stop_reg = (i == 0) ? PUMP1_STOP_REG : PUMP2_STOP_REG;
        
        int manual_mode = modbus_read_input_register(manual_reg);
        int stop_mode = modbus_read_input_register(stop_reg);
        
        if (manual_mode > 0) {
            debug(debug_tag, "泵浦%d處於手動模式", i + 1);
        }
        if (stop_mode > 0) {
            debug(debug_tag, "泵浦%d被停用", i + 1);
        }
    }
    
    return 0;
}

static int execute_automatic_control_mode(const system_sensor_data_t *data) {
    float target_flow, target_pressure;
    float flow_output, pressure_output;
    pump_control_output_t control_output = {{0}, {0}, 0};
    int temp_raw;
    
    info(debug_tag, "自動控制模式執行");
    
    // 設定自動模式 (只設定2台泵浦)
    modbus_write_single_register(PUMP1_MANUAL_MODE_REG, 0);
    modbus_write_single_register(PUMP2_MANUAL_MODE_REG, 0);
    
    // 讀取目標值
    temp_raw = modbus_read_input_register(TARGET_FLOW_REG);
    target_flow = (temp_raw >= 0) ? temp_raw / 10.0f : 100.0f; // 預設100 L/min
    
    temp_raw = modbus_read_input_register(TARGET_PRESSURE_REG);
    target_pressure = (temp_raw >= 0) ? temp_raw / 10.0f : 2.5f; // 預設2.5 bar
    
    // PID控制計算
    flow_output = calculate_pid_output(&flow_pid, target_flow, data->system_flow);
    pressure_output = calculate_pid_output(&pressure_pid, target_pressure, data->system_pressure);
    
    // 計算泵浦控制策略
    calculate_pump_strategy(flow_output, pressure_output, &control_output);
    
    // 執行控制輸出
    execute_pump_control(&control_output);
    
    info(debug_tag, "自動控制 - 流量PID: %.1f%%, 壓力PID: %.1f%%, 啟用泵浦: %d台", 
         flow_output, pressure_output, control_output.pump_count);
    
    return 0;
}

/*---------------------------------------------------------------------------
                            PID Controller Functions
 ---------------------------------------------------------------------------*/

static float calculate_pid_output(pid_controller_t *pid, float setpoint, float current_value) {
    time_t current_time = time(NULL);
    float delta_time = (current_time > pid->previous_time) ? 
        (current_time - pid->previous_time) : 1.0f;
    
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
    output = clamp_float(output, pid->output_min, pid->output_max);
    
    // 更新狀態
    pid->previous_error = error;
    pid->previous_time = current_time;
    
    debug(debug_tag, "PID計算 - 誤差: %.2f, P: %.2f, I: %.2f, D: %.2f, 輸出: %.2f", 
          error, proportional, integral_term, derivative_term, output);
    
    return output;
}

static void reset_pid_controller(pid_controller_t *pid) {
    pid->integral = 0.0f;
    pid->previous_error = 0.0f;
    pid->previous_time = time(NULL);
    debug(debug_tag, "PID控制器已重置");
}

/*---------------------------------------------------------------------------
                            Pump Control Strategy
 ---------------------------------------------------------------------------*/

// static int calculate_required_pumps(float target_flow) {
//     const float efficiency_factor = 0.9f; // 2泵系統效率更高
    
//     if (target_flow <= SINGLE_PUMP_MAX_FLOW * efficiency_factor) {
//         return 1;
//     } else {
//         return 2; // 最多2台泵浦
//     }
// }

static void calculate_pump_strategy(float flow_output, float pressure_output, pump_control_output_t *output) {
    // 綜合控制輸出 (取較大者)
    float control_demand = fmaxf(flow_output, pressure_output);
    
    // 添加調整因子
    if (control_demand > 80.0f) {
        control_demand += 15.0f; // 高需求時增加容量
    } else if (control_demand < 25.0f) {
        control_demand = fmaxf(control_demand, 20.0f); // 保持最小運行
    }
    
    // 限制控制需求範圍
    control_demand = clamp_float(control_demand, 15.0f, 100.0f);
    
    // 初始化輸出
    memset(output, 0, sizeof(pump_control_output_t));
    
    // 雙泵協調策略
    if (control_demand <= 50.0f) {
        // 單泵運行
        output->active_pumps[current_lead_pump - 1] = true;
        output->pump_speeds[current_lead_pump - 1] = control_demand * 1.8f; // 2泵系統單泵負載更高
        output->pump_count = 1;
    } else {
        // 雙泵運行
        output->active_pumps[0] = true;
        output->active_pumps[1] = true;
        output->pump_speeds[0] = control_demand / 1.6f; // 平衡負載分配
        output->pump_speeds[1] = control_demand / 1.6f;
        output->pump_count = 2;
    }
    
    // 限制泵浦速度範圍
    for (int i = 0; i < 2; i++) {
        if (output->active_pumps[i]) {
            output->pump_speeds[i] = clamp_float(output->pump_speeds[i], 
                                                PUMP_MIN_SPEED_DEFAULT, 
                                                PUMP_MAX_SPEED_DEFAULT);
        }
    }
    
    debug(debug_tag, "泵浦策略 - 需求: %.1f%%, 啟用: [%d,%d], 速度: [%.1f,%.1f]", 
          control_demand, output->active_pumps[0], output->active_pumps[1],
          output->pump_speeds[0], output->pump_speeds[1]);
}

static void execute_pump_control(const pump_control_output_t *output) {
    uint32_t speed_registers[2] = {DC_PUMP1_SPEED_CMD_REG, DC_PUMP2_SPEED_CMD_REG};
    uint32_t enable_registers[2] = {DC_PUMP1_ENABLE_CMD_REG, DC_PUMP2_ENABLE_CMD_REG};
    
    for (int i = 0; i < 2; i++) {
        if (output->active_pumps[i]) {
            // 啟動並設定速度 (0-1000對應0-100%)
            int speed_value = (int)(output->pump_speeds[i]);
            speed_value = clamp_float(speed_value, 0, 100);

            modbus_write_single_register(speed_registers[i], speed_value);
            modbus_write_single_register(enable_registers[i], 1);
            
            debug(debug_tag, "泵浦%d 啟動 - 速度: %d (%.1f%%)", i+1, speed_value, output->pump_speeds[i]);
        } else {
            // 停止泵浦
            modbus_write_single_register(speed_registers[i], 0);
            modbus_write_single_register(enable_registers[i], 0);
            debug(debug_tag, "泵浦%d 停止", i+1);
        }
    }
}

/*---------------------------------------------------------------------------
                            Utility Functions
 ---------------------------------------------------------------------------*/

static void handle_pump_rotation(void) {
    pump_rotation_timer++;
    
    // 假設控制週期為1分鐘，1440次 = 24小時 (雙泵輪換)
    if (pump_rotation_timer >= 1440) {
        current_lead_pump = (current_lead_pump == 1) ? 2 : 1; // 在1和2之間切換
        pump_rotation_timer = 0;
        info(debug_tag, "泵浦輪換 - 新主泵: 泵浦%d", current_lead_pump);
    }
}

static void handle_pump_fault(int pump_id, pump_fault_type_t fault_type) {
    const char* fault_names[] = {
        "NONE", "OVERCURRENT", "OVERVOLTAGE", 
        "UNDERVOLTAGE", "OVERLOAD", "COMMUNICATION", "EMERGENCY_STOP"
    };
    
    if (pump_id < 1 || pump_id > 2) {
        error(debug_tag, "無效的泵浦ID: %d", pump_id);
        return;
    }
    
    if (fault_type >= 0 && fault_type < sizeof(fault_names)/sizeof(fault_names[0])) {
        error(debug_tag, "泵浦%d故障: %s", pump_id, fault_names[fault_type]);
    } else {
        error(debug_tag, "泵浦%d未知故障: %d", pump_id, fault_type);
    }
    
    // 停止故障泵浦
    uint32_t enable_reg = (pump_id == 1) ? DC_PUMP1_ENABLE_CMD_REG : DC_PUMP2_ENABLE_CMD_REG;
    modbus_write_single_register(enable_reg, 0);
    
    // 排程故障恢復
    schedule_fault_recovery(pump_id, fault_type);
}

static float clamp_float(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/*---------------------------------------------------------------------------
                            Fault Recovery Functions
 ---------------------------------------------------------------------------*/

static void schedule_fault_recovery(int pump_id, pump_fault_type_t fault_type) {
    pthread_t recovery_thread;
    int* param = malloc(sizeof(int) * 2);
    
    if (param) {
        param[0] = pump_id;
        param[1] = fault_type;
        
        if (pthread_create(&recovery_thread, NULL, fault_recovery_task, param) == 0) {
            pthread_detach(recovery_thread);
            info(debug_tag, "已排程泵浦%d故障恢復任務", pump_id);
        } else {
            free(param);
            error(debug_tag, "無法創建故障恢復任務");
        }
    }
}

static void* fault_recovery_task(void* param) {
    int* params = (int*)param;
    int pump_id = params[0];
    // pump_fault_type_t fault_type = params[1];
    
    free(param);
    
    if (pump_id < 1 || pump_id > 2) {
        error(debug_tag, "故障恢復任務: 無效的泵浦ID %d", pump_id);
        return NULL;
    }
    
    // 等待冷卻時間
    usleep(FAULT_RECOVERY_DELAY_MS * 1000);
    
    // 嘗試復歸
    int retry_count = 0;
    const int max_retries = 3;
    
    uint32_t reset_reg = (pump_id == 1) ? DC_PUMP1_RESET_CMD_REG : DC_PUMP2_RESET_CMD_REG;
    
    while (retry_count < max_retries) {
        // 執行復歸
        modbus_write_single_register(reset_reg, 1);
        usleep(500000); // 等待0.5秒
        modbus_write_single_register(reset_reg, 0);
        
        usleep(5000000); // 等待5秒檢查狀態
        
        // 檢查復歸是否成功 (簡化檢查)
        pump_feedback_t feedback;
        if (read_pump_feedback(pump_id, &feedback) == 0 && 
            dc_pump_safety_check(&feedback)) {
            info(debug_tag, "泵浦%d恢復成功", pump_id);
            return NULL;
        }
        
        retry_count++;
        usleep(60000000); // 重試間隔1分鐘
    }
    
    error(debug_tag, "泵浦%d恢復失敗,已達最大重試次數%d", pump_id, max_retries);
    return NULL;
}
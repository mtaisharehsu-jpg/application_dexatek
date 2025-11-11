/**
 * @file control_logic_update.c
 * @brief 控制邏輯數據更新實現
 *
 * 本文件實現了控制邏輯系統的數據更新功能,負責定期從硬體讀取數據並更新到 Modbus 表。
 *
 * 主要功能:
 * 1. IO 板數據更新(數位輸入/輸出、模擬輸入/輸出)
 * 2. RTD 板數據更新(溫度、PWM 頻率/占空比)
 * 3. Modbus 設備數據更新(RS485 設備)
 * 4. RTC 時間數據更新
 * 5. Modbus 寄存器讀寫接口
 *
 * 實現原理:
 * - 使用多個執行緒定期更新不同類型的硬體數據
 * - IO 板更新執行緒: 更新數位 I/O 和模擬 I/O
 * - RTD 板更新執行緒: 更新溫度和 PWM 數據
 * - RTC 更新執行緒: 更新系統時間
 * - 數據更新後存儲到 Modbus 寄存器表中
 * - 支援數據類型轉換(電流轉流量、電流轉壓力等)
 *
 * 更新週期:
 * - IO/RTD 板: CONFIG_APPLICATION_CONTROL_LOGIC_UPDATE_DELAY_MS
 * - RTC: RTC_UPDATE_INTERVAL_MS (1000ms)
 *
 * @note 本文件是控制邏輯系統的數據採集層
 */

#include "dexatek/main_application/include/application_common.h"

#include "dexatek/main_application/managers/modbus_manager/modbus_manager.h"
#include "dexatek/main_application/managers/hid_manager/hid_manager.h"

#include "kenmec/main_application/kenmec_config.h"
#include "kenmec/main_application/control_logic/control_logic_manager.h"

#include <modbus.h>

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
/* 日誌標籤 */
static const char* tag = "control_logic_update";

/* RTC 更新間隔時間(毫秒) */
#define RTC_UPDATE_INTERVAL_MS (1000)

/* 控制邏輯更新調試開關 */
#define CONTROL_LOGIC_UPDATE_DEBUG_ENABLE 0

/*---------------------------------------------------------------------------
								Variables
 ---------------------------------------------------------------------------*/
/* IO 板更新執行緒句柄 */
static pthread_t _update_io_thread_handle = NULL;

/* RTD 板更新執行緒句柄 */
static pthread_t _update_rtd_thread_handle = NULL;

// static pthread_t _update_rtc_thread_handle = NULL;

/* RTC 更新使能標誌 */
BOOL _bUpdate_rtc_enable = TRUE;

/* 最後一次 RTC 更新時間戳 */
static uint64_t _latest_update_rtc_ts = 0;

/*---------------------------------------------------------------------------
                             Function Prototypes
 ---------------------------------------------------------------------------*/
int control_logic_modbus_manager_callback(uint16_t address, uint8_t type, uint32_t value);
int control_logic_1_config_update(uint16_t address, uint8_t type, uint32_t value);

/*---------------------------------------------------------------------------
                                 Implementation
 ---------------------------------------------------------------------------*/
static int _peripheral_DI_update(uint16_t port)
{
    int ret = SUCCESS;
    
    uint16_t target_base_address = 0;

    uint16_t status[8] = {0};

    switch (port) {
        case 0:
        case 1:
        case 2:
        case 3:
            ret = control_hardware_digital_input_all_get(port, status);
            if (ret == SUCCESS) {
                target_base_address = HID_BASE_ADDRESS + (port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_GPIO_INPUT_0;
                for (int i = 0; i < 8; i++) {
                    control_logic_update_to_modbus_table(target_base_address + i, MODBUS_TYPE_UINT16, &status[i]);
                }
            }
            break;
        default:
            break;
    }

    return ret;
}

static int _peripheral_DO_update(uint16_t port)
{
    int ret = SUCCESS;
    
    uint16_t target_base_address = 0;

    uint16_t status[8] = {0};

    switch (port) {
        case 0:
        case 1:
        case 2:
        case 3:
            ret = control_hardware_digital_output_all_get(port, status);
            if (ret == SUCCESS) {
                target_base_address = HID_BASE_ADDRESS + (port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_GPIO_OUTPUT_0;
                for (int i = 0; i < 8; i++) {
                    control_logic_update_to_modbus_table(target_base_address + i, MODBUS_TYPE_UINT16, &status[i]);
                }
            }
            break;
        default:
            break;
    }

    return ret;
}

static int _peripheral_AI_voltage_update(uint16_t port)
{
    int ret = SUCCESS;
    
    uint16_t target_base_address = HID_BASE_ADDRESS + (port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_A_VOLTAGE;

    int32_t mV[4] = {0};

    switch (port) {
        case 0:
        case 1:
        case 2:
        case 3:
            ret = control_hardware_analog_input_voltage_all_get(port, mV, 2000);
            if (ret == SUCCESS) {

                // analog voltage input covert by sensor type
                int config_count = 0;
                analog_config_t *ai_configs = control_logic_analog_input_voltage_configs_get(&config_count);

                for (int i = 0; i < 4; i++) {   
                    control_logic_update_to_modbus_table(target_base_address + (i * 2), MODBUS_TYPE_UINT32, &mV[i]);

                    // analog voltage input covert by sensor type
                    for (int j = 0; j < config_count; j++) {
                        if (ai_configs != NULL && ai_configs[j].port == port && ai_configs[j].channel == i) {
                            int32_t value = 0;
                            switch (ai_configs[j].sensor_type) {
                                case 0: // TODO: support sensor type 0
                                    value = mV[i];
                                    control_logic_update_to_modbus_table(ai_configs[j].update_address, MODBUS_TYPE_UINT16, &value);
                                    break;
                                case 1: // TODO: support sensor type 1
                                    value = mV[i];
                                    control_logic_update_to_modbus_table(ai_configs[j].update_address, MODBUS_TYPE_UINT16, &value);
                                    break;
                                default:
                                    error(tag, "Not supported sensor type %d", ai_configs[j].sensor_type);
                                    break;
                            }
                        }
                    }
                }
            } else {
                error(tag, "control_hardware_analog_input_voltage_all_get[%d] failed", port);
            }
            break;
        default:
            break;
    }

    return ret;
}

/**
 * @brief 電流轉換為水流量
 *
 * 功能說明:
 * 將 4-20mA 電流信號轉換為水流量值(0-100 LPM)。
 *
 * @param uA    輸入電流值(微安 uA)
 * @param value 輸出流量值指標
 *
 * @return int 執行結果
 *         - SUCCESS: 轉換成功
 *
 * 轉換公式:
 * - 4-20mA 對應 0-100LPM
 * - flow = (I - 4mA) * 6.25
 * - 輸出值放大10倍供 HMI 顯示
 *
 * 實現邏輯:
 * 1. 減去基準電流 4mA
 * 2. 計算流量值: (I - 4mA) / 1000 * 6.25
 * 3. 放大10倍供 HMI 顯示
 * 4. 轉換為整數返回
 */
static int _current_to_water_flow(int32_t uA, int32_t *value)
{
    /* flow = (I - 4mA) * 6.25 */
    /* 4-20mA -> 0-100LPM */

    int ret = SUCCESS;

    /* 減去基準電流 4mA */
    int32_t I_mA = uA - 4000;

    if (I_mA < 0) I_mA = 0;

    /* 計算流量 */
    float flow = ((float)I_mA / 1000.0f ) * 6.25f;

    /* 放大10倍供 HMI 顯示 */
    flow = flow * 10;

    /* 轉換為整數 */
    *value = (int32_t)lroundf(flow);

    return ret;
}

/**
 * @brief 電流轉換為壓力
 *
 * 功能說明:
 * 將 4-20mA 電流信號轉換為壓力值(0-10 bar)。
 *
 * @param uA    輸入電流值(微安 uA)
 * @param value 輸出壓力值指標
 *
 * @return int 執行結果
 *         - SUCCESS: 轉換成功
 *
 * 轉換公式:
 * - 4-20mA 對應 0-10 bar
 * - pressure = (I - 4mA) * 0.625
 * - 輸出值放大100倍供 HMI 顯示
 *
 * 實現邏輯:
 * 1. 減去基準電流 4mA
 * 2. 計算壓力值: (I - 4mA) / 1000 * 0.625
 * 3. 放大100倍供 HMI 顯示
 * 4. 轉換為整數返回
 */
static int _current_to_pressure(int32_t uA, int32_t *value)
{
    /* pressure = (I - 4mA) * 0.625 */
    /* 4-20mA -> 0-10 bar */

    int ret = SUCCESS;

    /* 減去基準電流 4mA */
    int32_t I_mA = uA - 4000;

    if (I_mA < 0) I_mA = 0;

    /* 計算壓力 */
    float pressure = ((float)I_mA / 1000.0f) * 0.625;

    /* 放大100倍供 HMI 顯示 */
    pressure = pressure * 100;

    *value = (int32_t)lroundf(pressure);

    return ret;
}

static int _peripheral_AI_current_update(uint16_t port)
{
    int ret = SUCCESS;
    
    uint16_t target_base_address = HID_BASE_ADDRESS + (port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_A_CURRENT;

    int32_t uA[4] = {0};

    switch (port) {
        case 0:
        case 1:
        case 2:
        case 3:
            ret = control_hardware_analog_input_current_all_get(port, uA, 2000);
            if (ret == SUCCESS) {
                // analog current input covert by sensor type
                int config_count = 0;
                analog_config_t *ai_configs = control_logic_analog_input_current_configs_get(&config_count);

                // for each port
                for (int i = 0; i < 4; i++) {
                    // update to modbus table
                    control_logic_update_to_modbus_table(target_base_address + (i * 2), MODBUS_TYPE_UINT32, &uA[i]);
                    
                    // analog current input covert by sensor type
                    for (int j = 0; j < config_count; j++) {
                        if (ai_configs != NULL && ai_configs[j].port == port && ai_configs[j].channel == i) {
                            int32_t value = 0;
                            switch (ai_configs[j].sensor_type) {
                                case 0: // water flow sensor
                                    _current_to_water_flow(uA[i], &value);
                                    // debug(tag, "port %d, ch %d, flow = %d", port, i, value);
                                    control_logic_update_to_modbus_table(ai_configs[j].update_address, MODBUS_TYPE_UINT16, &value);
                                    break;
                                case 1: // pressure sensor
                                    _current_to_pressure(uA[i], &value);
                                    // debug(tag, "port %d, ch %d, pressure = %d", port, i, value);
                                    control_logic_update_to_modbus_table(ai_configs[j].update_address, MODBUS_TYPE_UINT16, &value);
                                    break;
                                default:
                                    error(tag, "Not supported sensor type %d", ai_configs[j].sensor_type);
                                    break;
                            }
                        }
                    }
                }
            } else {
                error(tag, "control_hardware_analog_input_current_all_get[%d] failed", port);
            }
            break;
        default:
            break;
    }

    return ret;
}

static int _peripheral_AI_mode_update(uint16_t port)
{
    int ret = SUCCESS;
    
    uint16_t target_base_address = HID_BASE_ADDRESS + (port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_A_SET_MODE;
    
    uint16_t mode[4] = {0};

    switch (port) {
        case 0:
        case 1:
        case 2:
        case 3:
            ret = control_hardware_analog_mode_all_get(port, mode, 2000);
            if (ret == SUCCESS) {
                for (int i = 0; i < 4; i++) {
                    control_logic_update_to_modbus_table(target_base_address + i, MODBUS_TYPE_UINT16, &mode[i]);
                }
            } else {
                error(tag, "control_hardware_analog_input_current_all_get[%d] failed", port);
            }
            break;
        default:
            break;
    }

    return ret;
}

static int _peripheral_usb_info_update(uint16_t pid, uint16_t port)
{
    int ret = SUCCESS;
    
    uint16_t val = 0;
       
    uint16_t target_address = 0;

    switch (pid) {
        case HID_IO_BOARD_PID:
            // get port vid
            ret = hid_manager_device_vid_get(pid, port, &val);
            if (ret == SUCCESS) {
                target_address = HID_BASE_ADDRESS + (port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_BOARD_A_USB_VID;
                control_logic_update_to_modbus_table(target_address, MODBUS_TYPE_UINT16, &val);
            }
            // get port pid
            ret = hid_manager_device_pid_get(pid, port, &val);
            if (ret == SUCCESS) {
                target_address = HID_BASE_ADDRESS + (port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_BOARD_A_USB_PID;
                control_logic_update_to_modbus_table(target_address, MODBUS_TYPE_UINT16, &val);
            }            
            break;

        case HID_RTD_BOARD_PID:
            // get port vid
            ret = hid_manager_device_vid_get(pid, port, &val);
            if (ret == SUCCESS) {
                target_address = HID_BASE_ADDRESS + (port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_BOARD_B_USB_VID;
                control_logic_update_to_modbus_table(target_address, MODBUS_TYPE_UINT16, &val);
            }
            // get port pid
            ret = hid_manager_device_pid_get(pid, port, &val);
            if (ret == SUCCESS) {
                target_address = HID_BASE_ADDRESS + (port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_BOARD_B_USB_PID;
                control_logic_update_to_modbus_table(target_address, MODBUS_TYPE_UINT16, &val);
            }
            break;

        default:
            break;
    }

    return ret;
}

static int _peripheral_pwm_duty_update(uint16_t port)
{
    int ret = SUCCESS;
    
    uint16_t target_address = 0;

    uint16_t duty[8] = {0};

    switch (port) {
        case 0:
        case 1:
        case 2:
        case 3:
            ret = control_hardware_pwm_duty_all_get(port, 2000, duty);
            if (ret == SUCCESS) {
                for (int i = 0; i < 8; i++) {
                    target_address = HID_BASE_ADDRESS + (port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_CAPTURE_PWM_0_DUTY + i;
                    control_logic_update_to_modbus_table(target_address, MODBUS_TYPE_UINT16, &duty[i]);
                    // debug(tag, "port[%d] pwm%d_duty = %d", port, i, duty[i]);
                }
            } else {
                error(tag, "port[%d] control_hardware_pwm_duty_all_get failed", port);
            }
            break;
        default:
            break;
    }

    return ret;
}

static int _peripheral_pwm_freq_update(uint16_t port)
{
    int ret = SUCCESS;
    
    uint16_t target_address = 0;

    uint32_t freq[8] = {0};

    switch (port) {
        case 0:
        case 1:
        case 2:
        case 3:
            ret = control_hardware_pwm_freq_all_get(port, 2000, freq);
            if (ret == SUCCESS) {
                for (int i = 0; i < 8; i++) {
                    target_address = HID_BASE_ADDRESS + (port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_CAPTURE_PWM_0_FREQ + (i*2);
                    control_logic_update_to_modbus_table(target_address, MODBUS_TYPE_UINT32, &freq[i]);
                    // debug(tag, "port[%d] pwm%d_freq = %d", port, i, freq[i]);
                }
            } else {
                error(tag, "port[%d] control_hardware_pwm_freq_all_get failed", port);
            }
            break;
        default:
            break;
    }

    return ret;
}

static int _peripheral_pwm_period_update(uint16_t port)
{
    int ret = SUCCESS;
    
    uint16_t target_address = 0;

    uint32_t period[8] = {0};

    switch (port) {
        case 0:
        case 1:
        case 2:
        case 3:
            ret = control_hardware_pwm_period_all_get(port, period);
            if (ret == SUCCESS) {
                for (int i = 0; i < 8; i++) {
                    target_address = HID_BASE_ADDRESS + (port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_CAPTURE_PWM_0_PERIOD + (i*2);
                    control_logic_update_to_modbus_table(target_address, MODBUS_TYPE_UINT32, &period[i]);
                    // debug(tag, "port[%d] pwm%d_period = %d", port, i, period[i]);
                }
            } else {
                error(tag, "port[%d] control_hardware_pwm_period_all_get failed", port);
            }
            break;
        default:
            break;
    }

    return ret;
}

static int _peripheral_temperature_update(uint16_t port)
{
    int ret = SUCCESS;
    
    uint16_t target_address = 0;

    int32_t temp[8] = {0};

    switch (port) {
        case 0:
        case 1:
        case 2:
        case 3:
            ret = control_hardware_temperature_all_get(port, 2000, temp);
            if (ret == SUCCESS) {
                for (int i = 0; i < 8; i++) {
                    target_address = HID_BASE_ADDRESS + (port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD7124_CH_0_RESISTOR + (i*2);
                    control_logic_update_to_modbus_table(target_address, MODBUS_TYPE_INT32, &temp[i]);
                    // debug(tag, "port[%d] temp %d = %d", port, i, temp);
                }
            } else {
                error(tag, "port[%d] control_hardware_temperature_all_get failed", port);
            }
            break;
        default:
            break;
    }

    return ret;
}

static int _control_logic_io_boards_status_update(void)
{
    int ret = SUCCESS;
    
    // for each HID devices (IO, RTD)
    for (int port = 0; port < HID_DEVICES_MAX; port++) {
        uint16_t pid = 0;

        // get pid
        hid_manager_port_pid_get(port, &pid);

        // update peripheral data
        switch (pid) {
            case HID_IO_BOARD_PID:
                // debug(tag, "port[%d] pid = 0x%x", port, pid);
                _peripheral_DI_update(port);
                _peripheral_DO_update(port);
                _peripheral_AI_mode_update(port);
                _peripheral_AI_voltage_update(port);
                _peripheral_AI_current_update(port);
                // _peripheral_usb_info_update(pid, port);
                break;

            default:
                break;
        }
    }

    return ret;
}

static int _control_logic_rtd_boards_status_update(void)
{
    int ret = SUCCESS;
    
    // for each HID devices (IO, RTD)
    for (int port = 0; port < HID_DEVICES_MAX; port++) {
        uint16_t pid = 0;

        // get pid
        hid_manager_port_pid_get(port, &pid);

        // update peripheral data
        switch (pid) {
            case HID_RTD_BOARD_PID:
                // debug(tag, "port[%d] pid = 0x%x", i, pid);
                // _peripheral_pwm_duty_update(port);
                _peripheral_pwm_freq_update(port);
                // _peripheral_pwm_period_update(port);
                _peripheral_temperature_update(port);
                // _peripheral_usb_info_update(pid, port);
                break;

            default:
                break;
        }
    }

    return ret;
}

static int _control_logic_modbus_devices_update(void)
{
    int ret = SUCCESS;

    int modbus_device_config_count = 0;
    modbus_device_config_t *modbus_device_config = control_logic_modbus_device_configs_get(&modbus_device_config_count);

    if (modbus_device_config == NULL) {
        // error(tag, "modbus device config is not initialized");
        return FAIL;
    }

    // update modbus devices
    for (int i = 0; i < modbus_device_config_count; i++) {
        const modbus_device_config_t *cfg = &modbus_device_config[i];
        
        // debug(tag, "[%d] port:%d, slave:%u, addr:%u, name:%s", i, cfg->port, cfg->slave_id, cfg->reg_address, cfg->reg_name);

        // check USB port is connected and pid is RTD board
        uint16_t pid = 0;
        hid_manager_port_pid_get(cfg->port, &pid);
        if (pid != HID_RTD_BOARD_PID) {
            warn(tag, "cfg[%d] port:%d pid:0x%x != 0x%x, skip query", i, cfg->port, pid, HID_RTD_BOARD_PID);
            continue;
        }

        uint8_t query_register_num = 0;
        uint16_t query16_buffer[128] = {0};

        // check function code
        switch (cfg->function_code) {
            case MODBUS_FUNC_READ_COILS:
            case MODBUS_FUNC_READ_DISCRETE_INPUTS:
            case MODBUS_FUNC_READ_HOLDING_REGISTERS:
            case MODBUS_FUNC_READ_INPUT_REGISTERS:
                break;

            case MODBUS_FUNC_WRITE_SINGLE_REGISTER:
            case MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS:
                // error(tag, "skip write function code: %d", cfg->function_code);
                continue;

            default:
                error(tag, "Unsupported function code: %d", cfg->function_code);
                continue;
        }

        // get query register num
        switch (cfg->data_type) {
            case MODBUS_TYPE_INT16:
            case MODBUS_TYPE_UINT16:
            case MODBUS_TYPE_UINT16_LOWBYTE:
            case MODBUS_TYPE_UINT16_HIGHBYTE:
                query_register_num = 1;
                break;
            case MODBUS_TYPE_INT32:
            case MODBUS_TYPE_UINT32:
            case MODBUS_TYPE_FLOAT32:
                query_register_num = 2;
                break;
            case MODBUS_TYPE_UINT64:
                query_register_num = 4;
                break;
            default:
                error(tag, "Unsupported data type: %d", cfg->data_type);
                ret = FAIL;
                break;
        }

        // read data from modbus
        ret = control_hardware_rs485_multiple_read(cfg->port, cfg->baudrate, cfg->slave_id, cfg->function_code, cfg->reg_address, 
                                                   query_register_num, query16_buffer, 2000);

        // debug(tag, "[%d] port:%d, slave:%u, addr:%u, name:%s, ret:%d", i, cfg->port, cfg->slave_id, cfg->reg_address, cfg->name, ret);

        // update data to modbus manager
        if (ret == SUCCESS) {
            uint16_t update_address = cfg->update_address;

            // help_hexdump(query16_buffer, query_register_num, "query16_buffer");
            
            switch (cfg->data_type) {

                case MODBUS_TYPE_INT16: {
                    int16_t val = 0;
                    memcpy((uint8_t*)&val, (uint8_t*)query16_buffer, 2);

                    // scale
                    if (cfg->fScale != 0.0 && cfg->fScale != 1.0) {
                        val = (int16_t)(val * cfg->fScale);
                        // debug(tag, "[%d] val: %d, scale: %f", i, val, cfg->fScale);
                    }

                    // debug(tag, "[%d] update_address: %u, val: %d", i, update_address, val);
                    control_logic_update_to_modbus_table(update_address, MODBUS_TYPE_INT16, &val);
                    break;
                }

                case MODBUS_TYPE_UINT16: {
                    uint16_t val = 0;
                    memcpy((uint8_t*)&val, (uint8_t*)query16_buffer, 2);

                    // scale
                    if (cfg->fScale != 0.0 && cfg->fScale != 1.0) {
                        val = (uint16_t)(val * cfg->fScale);
                        // debug(tag, "[%d] val: %u, scale: %f", i, val, cfg->fScale);
                    }

                    // debug(tag, "[%d] update_address: %u, val: %u", i, update_address, val);
                    control_logic_update_to_modbus_table(update_address, MODBUS_TYPE_UINT16, &val);
                    break;
                }

                case MODBUS_TYPE_UINT16_LOWBYTE: {
                    uint16_t val = 0;
                    memcpy((uint8_t*)&val, (uint8_t*)query16_buffer, 2);
                    val = val & 0xFF;

                    // scale
                    if (cfg->fScale != 0.0 && cfg->fScale != 1.0) {
                        val = (uint16_t)(val * cfg->fScale);
                        // debug(tag, "[%d] val: %u, scale: %f", i, val, cfg->fScale);
                    }

                    // debug(tag, "[%d] update_address: %u, val: %u", i, update_address, val);
                    control_logic_update_to_modbus_table(update_address, MODBUS_TYPE_UINT16, &val);
                    break;
                }

                case MODBUS_TYPE_UINT16_HIGHBYTE: {
                    uint16_t val = 0;
                    memcpy((uint8_t*)&val, (uint8_t*)query16_buffer, 2);
                    val = val >> 8;

                    // scale
                    if (cfg->fScale != 0.0 && cfg->fScale != 1.0) {
                        val = (uint16_t)(val * cfg->fScale);
                        // debug(tag, "[%d] val: %u, scale: %f", i, val, cfg->fScale);
                    }

                    // debug(tag, "[%d] update_address: %u, val: %u", i, update_address, val);
                    control_logic_update_to_modbus_table(update_address, MODBUS_TYPE_UINT16, &val);
                    break;
                }

                case MODBUS_TYPE_INT32: {
                    int32_t val = 0;
                    memcpy((uint8_t*)&val, (uint8_t*)query16_buffer, 4);

                    // scale
                    if (cfg->fScale != 0.0 && cfg->fScale != 1.0) {
                        val = (int32_t)(val * cfg->fScale);
                        debug(tag, "[%d] val: %d, scale: %f", i, val, cfg->fScale);
                    }

                    // debug(tag, "[%d] update_address: %u, val: %d", i, update_address, val);
                    control_logic_update_to_modbus_table(update_address, MODBUS_TYPE_INT32, &val);
                    break;
                }

                case MODBUS_TYPE_UINT32: {
                    uint32_t val = 0;
                    memcpy((uint8_t*)&val, (uint8_t*)query16_buffer, 4);

                    // scale
                    if (cfg->fScale != 0.0 && cfg->fScale != 1.0) {
                        val = (uint32_t)(val * cfg->fScale);
                        debug(tag, "[%d] val: %u, scale: %f", i, val, cfg->fScale);
                    }

                    // debug(tag, "[%d] update_address: %u, val: %u", i, update_address, val);
                    control_logic_update_to_modbus_table(update_address, MODBUS_TYPE_UINT32, &val);
                    break;
                }

                case MODBUS_TYPE_FLOAT32: {
                    float val_f = 0;
                    memcpy((uint8_t*)&val_f, (uint8_t*)query16_buffer, 4);

                    // scale
                    if (cfg->fScale != 0.0 && cfg->fScale != 1.0) {
                        val_f = (float)(val_f * cfg->fScale);
                        debug(tag, "[%d] val: %f, scale: %f", i, val_f, cfg->fScale);
                    }

                    // debug(tag, "[%d] update_address: %u, val: %f", i, update_address, val_f);
                    control_logic_update_to_modbus_table(update_address, MODBUS_TYPE_FLOAT32, &val_f);      
                    break;
                }

                case MODBUS_TYPE_UINT64: {
                    uint64_t val = 0;
                    memcpy((uint8_t*)&val, (uint8_t*)query16_buffer, 8);

                    // scale
                    if (cfg->fScale != 0.0 && cfg->fScale != 1.0) {
                        val = (uint64_t)(val * cfg->fScale);
                        // debug(tag, "[%d] val: %llu, scale: %f", i, val, cfg->fScale);
                    }

                    // debug(tag, "[%d] update_address: %u, val: %llu", i, update_address, val);
                    control_logic_update_to_modbus_table(update_address, MODBUS_TYPE_UINT64, &val);
                    break;
                }

                default:
                    error(tag, "Unsupported destination type for device %d", i);
                    ret = FAIL;
                    break;
            }
        } else {
            error(tag, "RS485 read failed: dev %d, slave %u, addr %u", i, cfg->slave_id, (uint16_t)(cfg->reg_address));
            ret = FAIL;
        }

        // time_delay_ms(50);
    }

    return ret;
}

static void* _aio_boards_status_update_thread(void* arg)
{
    (void)arg;

    while (1) {
        time_delay_ms(CONFIG_APPLICATION_CONTROL_LOGIC_UPDATE_DELAY_MS);
#if defined(CONTROL_LOGIC_UPDATE_DEBUG_ENABLE) && CONTROL_LOGIC_UPDATE_DEBUG_ENABLE == 1
        uint64_t start_time = time_get_current_ms();
        debug(tag, "aio_update_thread +");
#endif
        _control_logic_io_boards_status_update();
#if defined(CONTROL_LOGIC_UPDATE_DEBUG_ENABLE) && CONTROL_LOGIC_UPDATE_DEBUG_ENABLE == 1
        uint64_t end_time = time_get_current_ms();
        debug(tag, "aio_update_thread - : %lld ms", end_time - start_time);
#endif
    }

    return NULL;
}

static void* _rtd_boards_status_update_thread(void* arg)
{
    (void)arg;

    while (1) {
        time_delay_ms(CONFIG_APPLICATION_CONTROL_LOGIC_UPDATE_DELAY_MS);
#if defined(CONTROL_LOGIC_UPDATE_DEBUG_ENABLE) && CONTROL_LOGIC_UPDATE_DEBUG_ENABLE == 1
        uint64_t start_time = time_get_current_ms();
        debug(tag, "rtd_update_thread +");
#endif
        _control_logic_rtd_boards_status_update();
        _control_logic_modbus_devices_update();
#if defined(CONTROL_LOGIC_UPDATE_DEBUG_ENABLE) && CONTROL_LOGIC_UPDATE_DEBUG_ENABLE == 1
        uint64_t end_time = time_get_current_ms();
        debug(tag, "rtd_update_thread - : %lld ms", end_time - start_time);
#endif
    }

    return NULL;
}

static void _control_logic_rtc_update(void)
{
    // get current time
    time_t now = time(NULL);
    
    // get localtime
    struct tm *tm_info = localtime(&now);

    if (tm_info != NULL) {
        // debug(tag, "update rtc: %d-%d-%d %d:%d:%d", tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

        uint16_t year = (uint16_t)(tm_info->tm_year + 1900);
        uint16_t month = (uint16_t)(tm_info->tm_mon + 1);
        uint16_t day = (uint16_t)tm_info->tm_mday;
        uint16_t hour = (uint16_t)tm_info->tm_hour;
        uint16_t min = (uint16_t)tm_info->tm_min;
        uint16_t sec = (uint16_t)tm_info->tm_sec;

        control_logic_update_to_modbus_table(MODBUS_ADDRESS_RTC_YEAR, MODBUS_TYPE_UINT16, &year);
        control_logic_update_to_modbus_table(MODBUS_ADDRESS_RTC_MONTH, MODBUS_TYPE_UINT16, &month);
        control_logic_update_to_modbus_table(MODBUS_ADDRESS_RTC_DAY, MODBUS_TYPE_UINT16, &day);
        control_logic_update_to_modbus_table(MODBUS_ADDRESS_RTC_HOUR, MODBUS_TYPE_UINT16, &hour);
        control_logic_update_to_modbus_table(MODBUS_ADDRESS_RTC_MIN, MODBUS_TYPE_UINT16, &min);
        control_logic_update_to_modbus_table(MODBUS_ADDRESS_RTC_SEC, MODBUS_TYPE_UINT16, &sec);
    } else {
        error(tag, "Failed to get localtime for RTC update");
    }
}


static void* _rtc_status_update_thread(void* arg)
{
    (void)arg;

    while (1) {
        uint64_t curr_ts = time_get_current_ms();
        
        // check ts is after RTC_UPDATE_INTERVAL_MS
        if (time_after(curr_ts - _latest_update_rtc_ts, RTC_UPDATE_INTERVAL_MS)) {
            _latest_update_rtc_ts = curr_ts;
            if (_bUpdate_rtc_enable == TRUE) {
                _control_logic_rtc_update();
            }
        }

        time_delay_ms(RTC_UPDATE_INTERVAL_MS);
    }

    return NULL;
}

/**
 * @brief 初始化控制邏輯更新模組
 *
 * 功能說明:
 * 初始化控制邏輯數據更新功能,創建數據更新執行緒。
 *
 * @return int 執行結果
 *         - SUCCESS: 初始化成功
 *         - FAIL: 初始化失敗
 *
 * 實現邏輯:
 * 1. 設置 Modbus 管理器的更新回調函數
 * 2. 創建 IO 板狀態更新執行緒
 * 3. 創建 RTD 板狀態更新執行緒
 * 4. (可選)創建 RTC 時間更新執行緒
 */
int control_logic_update_init(void)
{
    int ret = SUCCESS;

    /* 設置 Modbus 更新回調函數 */
    modbus_manager_update_callback_setup(control_logic_modbus_manager_callback);

    /* 創建 IO 板更新執行緒 */
    if (pthread_create(&_update_io_thread_handle, NULL, _aio_boards_status_update_thread, NULL) != 0) {
        error(tag, "Failed to create control logic update thread");
        ret = FAIL;
    }

    /* 創建 RTD 板更新執行緒 */
    if (pthread_create(&_update_rtd_thread_handle, NULL, _rtd_boards_status_update_thread, NULL) != 0) {
        error(tag, "Failed to create control logic rtd update thread");
        ret = FAIL;
    }

    /* 創建 RTC 時間更新執行緒 (目前停用) */
    // if (pthread_create(&_update_rtc_thread_handle, NULL, _rtc_status_update_thread, NULL) != 0) {
    //     error(tag, "Failed to create control logic rtc update thread");
    //     ret = FAIL;
    // }

    return ret;
}

static int _control_logic_rtc_set(void)
{
    int ret = SUCCESS;
    
    // Convert RTC year, month, day, hour, min, sec to epoch time and set system date
    uint16_t rtc_year = 0, rtc_month = 0, rtc_day = 0, rtc_hour = 0, rtc_min = 0, rtc_sec = 0;

    // Read all RTC registers from modbus table
    control_logic_load_from_modbus_table(MODBUS_ADDRESS_RTC_YEAR, MODBUS_TYPE_UINT16, &rtc_year);
    control_logic_load_from_modbus_table(MODBUS_ADDRESS_RTC_MONTH, MODBUS_TYPE_UINT16, &rtc_month);
    control_logic_load_from_modbus_table(MODBUS_ADDRESS_RTC_DAY, MODBUS_TYPE_UINT16, &rtc_day);
    control_logic_load_from_modbus_table(MODBUS_ADDRESS_RTC_HOUR, MODBUS_TYPE_UINT16, &rtc_hour);
    control_logic_load_from_modbus_table(MODBUS_ADDRESS_RTC_MIN, MODBUS_TYPE_UINT16, &rtc_min);
    control_logic_load_from_modbus_table(MODBUS_ADDRESS_RTC_SEC, MODBUS_TYPE_UINT16, &rtc_sec);

    struct tm rtc_tm;
    memset(&rtc_tm, 0, sizeof(struct tm));
    rtc_tm.tm_year = rtc_year - 1900; // tm_year is years since 1900
    rtc_tm.tm_mon = rtc_month - 1;    // tm_mon is 0-based
    rtc_tm.tm_mday = rtc_day;
    rtc_tm.tm_hour = rtc_hour;
    rtc_tm.tm_min = rtc_min;
    rtc_tm.tm_sec = rtc_sec;

    time_t epoch_time = mktime(&rtc_tm);

    debug(tag, "epoch_time = %ld", epoch_time);

    if (epoch_time != (time_t)-1) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "date -s @%ld", (long)epoch_time);
        ret = system(cmd);
        ret = system("hwclock -w");
        info(tag, "Set system date to epoch %ld (%d/%d/%d %d:%d:%d), ret = %d", (long)epoch_time, rtc_year, rtc_month, rtc_day, rtc_hour, rtc_min, rtc_sec, ret);
    } else {
        error(tag, "Failed to convert RTC to epoch time: Y:%d M:%d D:%d H:%d M:%d S:%d", rtc_year, rtc_month, rtc_day, rtc_hour, rtc_min, rtc_sec);
        ret = FAIL;
    }

    return ret;
}

int control_logic_modbus_manager_callback(uint16_t address, uint8_t type, uint32_t value)
{
    int ret = FAIL;

    BOOL bNeedSaveToFile = FALSE;

    // debug(tag, "control_logic_modbus_manager_callback: address %d, type %d, value %d", address, type, value);

    switch (address) {
        case MODBUS_ADDRESS_RTC_YEAR:
        case MODBUS_ADDRESS_RTC_MONTH:
        case MODBUS_ADDRESS_RTC_DAY:
        case MODBUS_ADDRESS_RTC_HOUR:
        case MODBUS_ADDRESS_RTC_MIN:
        case MODBUS_ADDRESS_RTC_SEC:
            // disable rtc update
            _bUpdate_rtc_enable = FALSE;
            // update modbus table first
            control_logic_update_to_modbus_table(address, type, &value);
            ret = _control_logic_rtc_set();
            // enable rtc update
            _bUpdate_rtc_enable = TRUE;
            bNeedSaveToFile = TRUE;
            break;

        default: {
            // Check if address is defined in device configs
            BOOL address_mapping_found = FALSE;
            int modbus_device_config_count = 0;
            modbus_device_config_t *modbus_device_config = control_logic_modbus_device_configs_get(&modbus_device_config_count);
            if (modbus_device_config != NULL) {
                for (int i = 0; i < modbus_device_config_count; i++) {
                    // debug(tag, "idx[%d], reg_address %d, function_code %d", i, _device_configs[i].reg_address, _device_configs[i].function_code);
                    if (modbus_device_config[i].update_address == address && 
                        modbus_device_config[i].function_code == MODBUS_FUNC_WRITE_SINGLE_REGISTER) {
                        address_mapping_found = TRUE;
                        ret = control_hardware_rs485_single_write(modbus_device_config[i].port, 
                                                                modbus_device_config[i].baudrate, 
                                                                modbus_device_config[i].slave_id, 
                                                                modbus_device_config[i].reg_address, 
                                                                value);                        
                        // debug(tag, "write to modbus success: address %d, value %d, ret %d", address, value, ret);
                        break;
                    }
                }
            }
            if (address_mapping_found) {
                info(tag, "address %d, type %d, value %d, bridge to 485 device, ret = %d", address, type, value, ret);
                bNeedSaveToFile = FALSE;
            } else {
                info(tag, "address %d, type %d, value %d, direct update to modbus table", address, type, value);
                control_logic_update_to_modbus_table(address, type, &value);
                ret = SUCCESS;
                bNeedSaveToFile = TRUE;
            }
            break;
        }
    }

    if (bNeedSaveToFile == TRUE) {
        ret = modbus_manager_data_mapping_save();
        debug(tag, "modbus_manager_data_mapping_save, ret = %d", ret);
    }

    return ret;
}

int control_logic_update_to_modbus_table(uint16_t address, uint8_t type, void *value)
{
    int ret = SUCCESS;

    // get modbus param
    modbus_mapping_t *mapping = modbus_manager_data_mapping_get();
    // debug(tag, "address: %d, type: %d, value: %d", address, type, *(uint16_t*)value);

    if (mapping == NULL) {
        error(tag, "modbus_mapping_t is NULL");
        ret = FAIL;
    } else if (value == NULL) {
        error(tag, "value pointer is NULL");
        ret = FAIL;
    } else if (address < mapping->start_registers || address >= mapping->start_registers + mapping->nb_registers) {
        error(tag, "address %d is out of range", address);
        ret = FAIL;
    } else {
        switch (type) {
            case MODBUS_TYPE_INT16:
                mapping->tab_registers[address] = *(int16_t*)value;
                break;

            case MODBUS_TYPE_UINT16:
                mapping->tab_registers[address] = *(uint16_t*)value;
                break;

            case MODBUS_TYPE_INT32: {
                int32_t val = *(int32_t*)value;
                mapping->tab_registers[address] = (uint16_t)(val & 0xFFFF);
                mapping->tab_registers[address+1] = (uint16_t)((val >> 16) & 0xFFFF);
                break;
            }

            case MODBUS_TYPE_UINT32: {
                uint32_t val = *(uint32_t*)value;
                mapping->tab_registers[address] = (uint16_t)(val & 0xFFFF);
                mapping->tab_registers[address+1] = (uint16_t)((val >> 16) & 0xFFFF);
                break;
            }

            case MODBUS_TYPE_FLOAT32: {
                union {
                    float f;
                    uint32_t u32;
                } float_converter;
                float_converter.f = *(float*)value;
                mapping->tab_registers[address] = (uint16_t)(float_converter.u32 & 0xFFFF);
                mapping->tab_registers[address+1] = (uint16_t)((float_converter.u32 >> 16) & 0xFFFF);
                break;
            }

            case MODBUS_TYPE_UINT64: {
                // TODO: check if this is correct
                uint64_t val = *(uint64_t*)value;
                mapping->tab_registers[address] = (uint16_t)(val & 0xFFFF);
                mapping->tab_registers[address+1] = (uint16_t)((val >> 16) & 0xFFFF);
                mapping->tab_registers[address+2] = (uint16_t)((val >> 32) & 0xFFFF);
                mapping->tab_registers[address+3] = (uint16_t)((val >> 48) & 0xFFFF);
                break;
            }

            default:
                error(tag, "invalid type: %d", type);
                ret = FAIL;
                break;
        }
    }

    return ret;
}

int control_logic_load_from_modbus_table(uint16_t address, uint8_t type, void *data)
{
    int ret = SUCCESS;

    // get modbus param
    modbus_mapping_t *mapping = modbus_manager_data_mapping_get();

    // debug(tag, "control_logic_update_to_modbus_manager: %d, type: %d", address, type);

    if (mapping == NULL) {
        error(tag, "modbus_mapping_t is NULL");
        ret = FAIL;
    } else if (data == NULL) {
        error(tag, "data pointer is NULL");
        ret = FAIL;
    } else if (address < mapping->start_registers || address >= mapping->start_registers + mapping->nb_registers) {
        error(tag, "address %d is out of range", address);
        ret = FAIL;
    } else {
        switch (type) {
            case MODBUS_TYPE_INT16:
                *(int16_t*)data = mapping->tab_registers[address];
                break;

            case MODBUS_TYPE_UINT16:
                *(uint16_t*)data = mapping->tab_registers[address];
                break;

            case MODBUS_TYPE_INT32:
            case MODBUS_TYPE_UINT32: {
                AppConvertUint32_16 conv;
                conv.words.u16_byte[0] = mapping->tab_registers[address];
                conv.words.u16_byte[1] = mapping->tab_registers[address+1];
                memcpy((uint8_t*)data, (uint8_t*)&conv, 4);
                break;
            }

            case MODBUS_TYPE_FLOAT32: {
                AppConvertUint32_16 conv;
                conv.words.u16_byte[0] = mapping->tab_registers[address];
                conv.words.u16_byte[1] = mapping->tab_registers[address+1];
                memcpy((uint8_t*)data, (uint8_t*)&conv, 4);
                break;
            }

            case MODBUS_TYPE_UINT64: {
                // TODO: check if this is correct
                AppConvert64_16 conv;
                conv.words.u16_byte[0] = mapping->tab_registers[address];
                conv.words.u16_byte[1] = mapping->tab_registers[address+1];
                conv.words.u16_byte[2] = mapping->tab_registers[address+2];
                conv.words.u16_byte[3] = mapping->tab_registers[address+3];
                memcpy((uint8_t*)data, (uint8_t*)&conv, 8);
                break;
            }

            default:
                error(tag, "invalid type: %d", type);
                ret = FAIL;
                break;
        }
    }

    return ret;
}

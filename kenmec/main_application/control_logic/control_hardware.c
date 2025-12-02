/**
 * @file control_hardware.c
 * @brief 控制硬體接口實現
 *
 * 本文件實現了控制邏輯系統的硬體控制接口,提供對各種硬體設備的訪問。
 *
 * 主要功能:
 * 1. RS485 設備通訊(Modbus協議)
 * 2. 數位輸入/輸出控制(GPIO)
 * 3. 模擬輸入控制(電壓/電流)
 * 4. 模擬輸出控制(電壓/電流)
 * 5. 溫度測量(RTD/PT100/PT1000)
 * 6. PWM 輸入/輸出控制
 * 7. AI/AO 模式設置
 *
 * 實現原理:
 * - 通過 HID 管理器與硬體設備通訊
 * - 支援多個 IO 板和 RTD 板
 * - 提供統一的硬體抽象層接口
 * - 支援從硬體讀取或從 RAM 緩存讀取
 * - 使用 DK Modbus 協議與 HID 設備通訊
 *
 * 硬體設備類型:
 * - IO 板(0xA2): GPIO、AD74416H(AI/AO)
 * - RTD 板(0xA3): AD7124(RTD)、PWM
 *
 * @note 本文件是控制邏輯系統的硬體抽象層
 */

#include "dexatek/main_application/include/application_common.h"

#include "dexatek/main_application/managers/hid_manager/hid_manager.h"
#include "dexatek/main_application/managers/hid_manager/dk_modbus_gpio.h"
#include "dexatek/main_application/managers/hid_manager/dk_modbus_cap_pwm.h"
#include "dexatek/main_application/managers/hid_manager/dk_modbus_ad7124.h"
#include "dexatek/main_application/managers/hid_manager/dk_modbus_setting.h"
#include "dexatek/main_application/managers/hid_manager/dk_modbus_pwm.h"
#include "dexatek/main_application/managers/hid_manager/dk_modbus_ad74416h.h"

#include "dexatek/main_application/managers/modbus_manager/modbus_manager.h"

#include "kenmec/main_application/control_logic/control_logic_manager.h"

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
/* 日誌標籤 */
 static const char* tag = "cl_hardware";
 
 /*---------------------------------------------------------------------------
                             Function Prototypes
  ---------------------------------------------------------------------------*/
/**
 * @brief PT100 電阻轉換為溫度
 * @param resistance_mohm 電阻值(毫歐姆)
 * @return float 溫度值(攝氏度)
 */
static float _pt100_resistance_to_temp_float(uint32_t resistance_mohm);

 /*---------------------------------------------------------------------------
                                 Implementation
  ---------------------------------------------------------------------------*/

/**
 * @brief PT100 電阻轉換為溫度(簡化版)
 *
 * 功能說明:
 * 使用簡化公式將 PT100 電阻值轉換為溫度值。
 *
 * @param resistance_mohm 電阻值(毫歐姆 mΩ)
 *
 * @return float 溫度值(攝氏度)
 *
 * 轉換公式:
 * - PT100 基準電阻: R0 = 100Ω = 100000 mΩ
 * - 溫度係數: 0.00385 Ω/Ω/°C
 * - temp = (R - R0) * 10 / 385 / 10
 */
static float _pt100_resistance_to_temp_float(uint32_t resistance_mohm)
{
    /* PT100: R0 = 100Ω = 100000 mΩ */
    int delta = resistance_mohm - 100000;
    if (delta < 0) delta = 0;

    /* 溫度計算(放大10倍) */
    int temp_x10 = (delta * 10) / 385;

    return temp_x10 / 10.0f;
}

/**
 * @brief 電阻轉換為溫度(通用版)
 *
 * 功能說明:
 * 使用標準公式將 RTD 電阻值轉換為溫度值,支援 PT100/PT1000 等不同基準電阻。
 *
 * @param resistance_mohm 電阻值(毫歐姆 mΩ)
 * @param base_resistance 基準電阻(歐姆 Ω)
 *                        - 100: PT100
 *                        - 1000: PT1000
 *
 * @return float 溫度值(攝氏度)
 *
 * 轉換公式:
 * - T = (R - R0) / (A * R0)
 * - A = 0.00385 (溫度係數)
 */
static float _resistance_to_temperature(uint32_t resistance_mohm, uint16_t base_resistance)
{
    /* 基準電阻轉換為毫歐姆 */
    float R0_mohm = base_resistance * 1000.0f;
    /* 溫度係數 */
    float A = 0.00385f;
    float temp = 0.0f;

    /* T = (R - R0) / (A * R0) */
    temp = ((float)resistance_mohm - R0_mohm) / (A * R0_mohm);

    /* 電阻為0時溫度設為0 */
    if (resistance_mohm == 0) {
        temp = 0.0f;
    }

    return temp;
}

int control_hardware_rs485_pressure_get(uint8_t hid_port, uint16_t baudrate, uint8_t slave_id, uint8_t function_code, 
    uint16_t address, float *pressure, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;

    *pressure = 0.0f;

    uint8_t packet[64];
    uint8_t recvbuf[64];

    CModbusUartBaudrate(hid_pid, hid_port, baudrate);

    CModbusReadPacketEx(packet, slave_id, function_code, address, 1);

    hid_manager_write(hid_pid, hid_port, packet, 64, timeout_ms);
    // if (ret_write < 0) {
        // return FAIL;
    // }

    int ret_read = hid_manager_read(hid_pid, hid_port, recvbuf, 64, timeout_ms);    
    if (ret_read >= 0) {
        // CModbusReadLength(recvbuf);
        uint8_t* content = CModbusReadContent(recvbuf);
        // debug(tag, "content[0] = %d, content[1] = %d", content[0], content[1]);
        *pressure = ((content[0] * 256 + content[1]) / 100.0f);
        ret = SUCCESS;
    } else {
        ret = FAIL;
    }

    return ret;
}

int control_hardware_rs485_multiple_read(uint8_t hid_port, uint32_t baudrate, uint8_t slave_id, uint8_t function_code, 
        uint16_t address, uint16_t quantity, uint16_t *values, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;

    // Calculate maximum quantity that fits in 64-byte packet
    // Modbus RTU frame: slave_id(1) + function(1) + start_addr(2) + quantity(2) + crc(2) = 8 bytes overhead
    // Each register response is 2 bytes, so max data = 64 - 8 = 56 bytes = 28 registers
    const uint16_t max_quantity_per_packet = 28;
    
    uint16_t remaining = quantity;
    uint16_t current_address = address;
    uint16_t values_index = 0;

    while (remaining > 0) {
        uint16_t current_quantity = (remaining > max_quantity_per_packet) ? max_quantity_per_packet : remaining;
        
        uint8_t packet[64];
        uint8_t recvbuf[64];

        // debug(tag, "current_address = %d, current_quantity = %d", current_address, current_quantity);

        CModbusUartBaudrate(hid_pid, hid_port, baudrate);

        CModbusReadPacketEx(packet, slave_id, function_code, current_address, current_quantity);

        hid_manager_write(hid_pid, hid_port, packet, 64, timeout_ms);
        // debug(tag, "ret_write = %d", ret_write);
        // if (ret_write < 0) {
            // return FAIL;
        // }

        int ret_read = hid_manager_read(hid_pid, hid_port, recvbuf, 64, timeout_ms);
        // debug(tag, "ret_read = %d", ret_read);
        // help_hexdump(recvbuf, 64, "recvbuf");
        if (ret_read >= 0) {
            uint8_t* content = CModbusReadContent(recvbuf);
            
            // Parse the current batch of 16-bit values from the response
            for (uint16_t i = 0; i < current_quantity; i++) {
                switch (function_code) {
                    case MODBUS_FUNC_READ_COILS:
                        values[values_index + i] = (content[i * 2]);
                        break;
                    case MODBUS_FUNC_READ_DISCRETE_INPUTS:
                        values[values_index + i] = (content[i * 2]);
                        break;
                    default:
                        values[values_index + i] = (content[i * 2] * 256 + content[i * 2 + 1]);
                        break;
                }
            }
        } else {
            ret = FAIL;
            break;
        }

        remaining -= current_quantity;
        current_address += current_quantity;
        values_index += current_quantity;
    }

    return ret;
}

int control_hardware_rs485_single_read(uint8_t hid_port, uint16_t baudrate, uint8_t slave_id, uint8_t function_code, 
    uint16_t address, uint16_t *val, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;

    *val = 0;

    uint8_t packet[64];
    uint8_t recvbuf[64];

    CModbusUartBaudrate(hid_pid, hid_port, baudrate);

    CModbusReadPacketEx(packet, slave_id, function_code, address, 1);

    hid_manager_write(hid_pid, hid_port, packet, 64, timeout_ms);
    // if (ret_write < 0) {
        // return FAIL;
    // }

    int ret_read = hid_manager_read(hid_pid, hid_port, recvbuf, 64, timeout_ms);    
    if (ret_read >= 0) {
        // CModbusReadLength(recvbuf);
        uint8_t* content = CModbusReadContent(recvbuf);
        *val = (content[0] * 256 + content[1]);
        ret = SUCCESS;
    } else {
        ret = FAIL;
    }

    return ret;
}

int control_hardware_rs485_single_write(uint8_t hid_port, uint16_t baudrate, uint8_t slave_id, uint16_t address, uint16_t val)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;

    uint8_t packet[64];
    uint8_t recvbuf[64];

    CModbusUartBaudrate(hid_pid, hid_port, baudrate);

    CModbusWritePacket(packet, slave_id, address, val);

    hid_manager_write(hid_pid, hid_port, packet, 64, 1000);
    // if (ret_write < 0) {
        // return FAIL;
    // }

    int ret_read = hid_manager_read(hid_pid, hid_port, recvbuf, 64, 1000);
    if (ret_read < 0) {
        ret = FAIL;
    }

    return ret;
}

int control_hardware_analog_input_current_get(uint8_t hid_port, uint8_t channel, int32_t *uA, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;
    uint16_t address = 0;
    
    *uA = 0;
    
    // channel to analog input address
    switch (channel) {
        case 0:
            address = DK_MODBUS_AD74416H_GET_CURRENT_INPUT_CH_A;
            break;
        case 1:
            address = DK_MODBUS_AD74416H_GET_CURRENT_INPUT_CH_B;
            break;
        case 2:
            address = DK_MODBUS_AD74416H_GET_CURRENT_INPUT_CH_C;
            break;
        case 3:
            address = DK_MODBUS_AD74416H_GET_CURRENT_INPUT_CH_D;
            break;
        default:
            error(tag, "invalid channel: %d", channel);
            ret = FAIL;
            break;
    }

    ret = CModbusAD74416hGetInput(hid_pid, hid_port, address, 1, uA, timeout_ms);
    // debug(tag, "[port %d] ch %d, AI_value = %d (uA), ret = %d", hid_port, channel, *uA, ret);

    return ret;
}

int control_hardware_analog_input_current_all_get(uint8_t hid_port, int32_t uA[4], uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;
    uint16_t address = DK_MODBUS_AD74416H_GET_CURRENT_INPUT_CH_A;
    
    uA[0] = 0;
    uA[1] = 0;
    uA[2] = 0;
    uA[3] = 0;
    
    ret = CModbusAD74416hGetInput(hid_pid, hid_port, address, 4, uA, timeout_ms);
    
    return ret;
}

int control_hardware_analog_mode_all_get(uint8_t hid_port, uint16_t mode[4], uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;
    uint16_t address = DK_MODBUS_AD74416H_SET_MODE_CH_A;
    
    mode[0] = 0;
    mode[1] = 0;
    mode[2] = 0;
    mode[3] = 0;
    
    ret = CModbusAD74416hGetMode(hid_pid, hid_port, address, 4, mode, timeout_ms);
    // debug(tag, "port %d, mode[0] = %d, mode[1] = %d, mode[2] = %d, mode[3] = %d", hid_port, mode[0], mode[1], mode[2], mode[3]);
    
    return ret;
}

int control_hardware_analog_input_current_get_from_ram(uint8_t hid_port, uint8_t channel, int32_t *mA)
{
    int ret = SUCCESS;
    
    *mA = 0;

    uint16_t address =  0;

    // channel to analog input address
    switch (channel) {
        case 0:
            address = HID_BASE_ADDRESS + (hid_port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_A_CURRENT;
            break;
        case 1:
            address = HID_BASE_ADDRESS + (hid_port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_B_CURRENT;
            break;
        case 2:
            address = HID_BASE_ADDRESS + (hid_port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_C_CURRENT;
            break;
        case 3:
            address = HID_BASE_ADDRESS + (hid_port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_D_CURRENT;
            break;
        default:
            error(tag, "invalid channel: %d", channel);
            ret = FAIL;
            break;
    }

    // get current from ram
    if (ret == SUCCESS) {
        modbus_mapping_t *mapping = modbus_manager_data_mapping_get();
        if (mapping != NULL) {
            AppConvertUint32_16 conv;
            conv.words.u16_byte[0] = mapping->tab_registers[address];
            conv.words.u16_byte[1] = mapping->tab_registers[address+1];
            *mA = conv.val;
        } else {
            ret = FAIL;
        }
    } else {
        error(tag, "get current from ram failed");
    }

    return ret;
}

int control_hardware_analog_input_voltage_get(uint8_t hid_port, uint8_t channel, int32_t *mV, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;
    uint16_t address = 0;
    
    *mV = 0;
    
    // channel to analog input address
    switch (channel) {
        case 0:
            address = DK_MODBUS_AD74416H_GET_VOLTAGE_INPUT_CH_A;
            break;
        case 1:
            address = DK_MODBUS_AD74416H_GET_VOLTAGE_INPUT_CH_B;
            break;
        case 2:
            address = DK_MODBUS_AD74416H_GET_VOLTAGE_INPUT_CH_C;
            break;
        case 3:
            address = DK_MODBUS_AD74416H_GET_VOLTAGE_INPUT_CH_D;
            break;
        default:
            error(tag, "invalid channel: %d", channel);
            ret = FAIL;
            break;
    }

    ret = CModbusAD74416hGetInput(hid_pid, hid_port, address, 1, mV, timeout_ms);
    // debug(tag, "[port %d] ch %d, AI_value = %d (mA), ret = %d", hid_port, channel, *mA, ret);

    return ret;
}

int control_hardware_analog_input_voltage_all_get(uint8_t hid_port, int32_t mV[4], uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;
    uint16_t address = DK_MODBUS_AD74416H_GET_VOLTAGE_INPUT_CH_A;
    
    mV[0] = 0;
    mV[1] = 0;
    mV[2] = 0;
    mV[3] = 0;
    
    ret = CModbusAD74416hGetInput(hid_pid, hid_port, address, 4, mV, timeout_ms);

    return ret;
}

int control_hardware_analog_input_voltage_get_from_ram(uint8_t hid_port, uint8_t channel, int32_t *mV)
{
    int ret = SUCCESS;
    
    *mV = 0;

    uint16_t address =  0;

    // channel to analog input address
    switch (channel) {
        case 0:
            address = HID_BASE_ADDRESS + (hid_port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_A_VOLTAGE;
            break;
        case 1:
            address = HID_BASE_ADDRESS + (hid_port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_B_VOLTAGE;
            break;
        case 2:
            address = HID_BASE_ADDRESS + (hid_port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_C_VOLTAGE;
            break;
        case 3:
            address = HID_BASE_ADDRESS + (hid_port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_D_VOLTAGE;
            break;
        default:
            error(tag, "invalid channel: %d", channel);
            ret = FAIL;
            break;
    }

    // get current from ram
    if (ret == SUCCESS) {
        modbus_mapping_t *mapping = modbus_manager_data_mapping_get();
        if (mapping != NULL) {
            AppConvertUint32_16 conv;
            conv.words.u16_byte[0] = mapping->tab_registers[address];
            conv.words.u16_byte[1] = mapping->tab_registers[address+1];
            *mV = conv.val;
        } else {
            ret = FAIL;
        }
    } else {
        error(tag, "get current from ram failed");
    }

    return ret;
}

int control_hardware_analog_output_current_set(uint8_t hid_port, uint8_t channel, uint32_t val, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;
    uint16_t address = 0;
    
    // channel to analog input address
    switch (channel) {
        case 0:
            address = DK_MODBUS_AD74416H_CURRENT_OUTPUT_CH_A;
            break;
        case 1:
            address = DK_MODBUS_AD74416H_CURRENT_OUTPUT_CH_B;
            break;
        case 2:
            address = DK_MODBUS_AD74416H_CURRENT_OUTPUT_CH_C;
            break;
        case 3:
            address = DK_MODBUS_AD74416H_CURRENT_OUTPUT_CH_D;
            break;
        default:
            error(tag, "invalid channel: %d", channel);
            ret = FAIL;
            break;
    }

    ret = CModbusAD74416hCurrentOutput(hid_pid, hid_port, address, val, timeout_ms);
    // debug(tag, "[port %d] ch %d, AI_value = %d (mA), ret = %d", hid_port, channel, *mA, ret);

    return ret;
}


int control_hardware_analog_output_voltage_set(uint8_t hid_port, uint8_t channel, uint32_t val, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;
    uint16_t address = 0;
    
    // channel to analog input address
    switch (channel) {
        case 0:
            address = DK_MODBUS_AD74416H_VOLTAGE_OUTPUT_CH_A;
            break;
        case 1:
            address = DK_MODBUS_AD74416H_VOLTAGE_OUTPUT_CH_B;
            break;
        case 2:
            address = DK_MODBUS_AD74416H_VOLTAGE_OUTPUT_CH_C;
            break;
        case 3:
            address = DK_MODBUS_AD74416H_VOLTAGE_OUTPUT_CH_D;
            break;
        default:
            error(tag, "invalid channel: %d", channel);
            ret = FAIL;
            break;
    }

    ret = CModbusAD74416hVoltageOutput(hid_pid, hid_port, address, val, timeout_ms);

    return ret;
}

int control_hardware_digital_input_get(uint8_t hid_port, uint8_t channel, uint16_t *value)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;

    *value = 0;

    // channel to digital input address
    uint16_t digital_input_address = DK_MODBUS_GPIO_INPUT_0 + channel;

    ret = CModbusGPIOStatus(hid_pid, hid_port, digital_input_address, 1, value);
    // debug(tag, "[port %d] DI[%d] = %d, ret = %d", hid_port, channel, *value, ret);

    return ret;
}

int control_hardware_digital_input_all_get(uint8_t hid_port, uint16_t value[8])
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;

    // channel to digital input address
    uint16_t digital_input_address = DK_MODBUS_GPIO_INPUT_0;

    ret = CModbusGPIOStatus(hid_pid, hid_port, digital_input_address, 8, value);
    // debug(tag, "[port %d] DI[%d] = %d, ret = %d", hid_port, channel, *value, ret);

    return ret;
}

int control_hardware_digital_input_all_get_from_ram(uint8_t hid_port, uint16_t value[8])
{
    int ret = SUCCESS;
    
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;
    value[3] = 0;
    value[4] = 0;
    value[5] = 0;
    value[6] = 0;
    value[7] = 0;

    uint16_t address =  HID_BASE_ADDRESS + (hid_port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_GPIO_INPUT_0;

    modbus_mapping_t *mapping = modbus_manager_data_mapping_get();
    if (mapping != NULL) {
        for (int i = 0; i < 8; i++) {
            value[i] = mapping->tab_registers[address + i];
        }
    } else {
        ret = FAIL;
    }

    return ret;
}

int control_hardware_analog_mode_all_get_from_ram(uint8_t hid_port, uint16_t value[4])
{
    int ret = SUCCESS;
    
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;
    value[3] = 0;

    uint16_t address =  HID_BASE_ADDRESS + (hid_port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD74416H_CH_A_SET_MODE;

    modbus_mapping_t *mapping = modbus_manager_data_mapping_get();
    if (mapping != NULL) {
        for (int i = 0; i < 4; i++) {
            value[i] = mapping->tab_registers[address + i];
        }
    } else {
        ret = FAIL;
    }

    return ret;
}

int control_hardware_digital_output_all_get(uint8_t hid_port, uint16_t value[8])
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;
    
    // channel to digital output address
    uint16_t address = DK_MODBUS_GPIO_OUTPUT_0;

    ret = CModbusGPIOStatus(hid_pid, hid_port, address, 8, value);
    // debug(tag, "[port %d] DO[%d] = %d, ret = %d", hid_port, channel, value, ret);

    return ret;
}

int control_hardware_digital_output_all_get_from_ram(uint8_t hid_port, uint16_t value[8])
{
    int ret = SUCCESS;
    
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;
    value[3] = 0;
    value[4] = 0;
    value[5] = 0;
    value[6] = 0;
    value[7] = 0;

    uint16_t address =  HID_BASE_ADDRESS + (hid_port * HID_IO_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_GPIO_OUTPUT_0;

    modbus_mapping_t *mapping = modbus_manager_data_mapping_get();
    if (mapping != NULL) {
        for (int i = 0; i < 8; i++) {
            value[i] = mapping->tab_registers[address + i];
        }
    } else {
        ret = FAIL;
    }

    return ret;
}

int control_hardware_digital_output_set(uint8_t hid_port, uint8_t channel, uint16_t value, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;

    // channel to digital output address
    uint16_t digital_output_address = DK_MODBUS_GPIO_OUTPUT_0 + channel;

    ret = CModbusGPIOOutput(hid_pid, hid_port, digital_output_address, value, timeout_ms);
    // debug(tag, "[port %d] DO[%d] = %d, ret = %d", hid_port, channel, value, ret);

    return ret;
}

int control_hardware_digital_output_all_set(uint8_t hid_port, uint16_t val)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;
        
    ret = CModbusGPIOOutputAll(hid_pid, hid_port, val, val, val, val, val, val, val, val);
    // debug(tag, "[port %d] gpio_output = %d, ret = %d", hid_port, out, ret);

    return ret;
}

int control_hardware_temperature_get(uint8_t hid_port, uint8_t channel, uint16_t timeout_ms, float *temp_float) 
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;
    uint32_t read_value = 0;

    // channel to rtd address
    uint16_t rtd_address = DK_MODBUS_AD7124_GET_RESISTANCE_CH_0 + channel * 2;

    *temp_float = 0.0f;
    
    // get resistance
    ret = CModbusAD7124GetResistance(hid_pid, hid_port, rtd_address, 1, &read_value, timeout_ms);
    if (ret == SUCCESS) {
        // resistance to temperature
        uint32_t resistance_mohm = read_value*10; // 0.01 ohm to mohm
        *temp_float = _pt100_resistance_to_temp_float(resistance_mohm);
        // debug(tag, "[port %d] RTD_%d_temp = %.2f, ret = %d", hid_port, 0, *temp_float, ret);
    }

    return ret;
}

int control_hardware_temperature_all_get(uint8_t hid_port, uint16_t timeout_ms, int32_t temperature[8]) 
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;
    uint32_t read_value[8] = {0};

    // channel to rtd address
    uint16_t rtd_address = DK_MODBUS_AD7124_GET_RESISTANCE_CH_0;

    float temp_float = 0.0f;
    
    // get resistance
    ret = CModbusAD7124GetResistance(hid_pid, hid_port, rtd_address, 8, read_value, timeout_ms);
    if (ret == SUCCESS) {
        int config_count = 0;
        temperature_config_t *temperature_configs = control_logic_temperature_configs_get(&config_count);
        // for each channel
        for (int i = 0; i < 8; i++) {
            int16_t update_address = -1;
            uint8_t temperature_sensor_type = 0;
    
            // get temperature sensor type
            for (int j = 0; j < config_count; j++) {
                if (temperature_configs != NULL) {
                    if (temperature_configs[j].port == hid_port && temperature_configs[j].channel == i) {
                        update_address = temperature_configs[j].update_address;
                        temperature_sensor_type = temperature_configs[j].sensor_type;
                        break;
                    }
                }
            }
            // 0.01 ohm to mohm
            uint32_t resistance_mohm = read_value[i]*10; // 0.01 ohm to mohm
            switch (temperature_sensor_type) {
                default:
                case 0: // PT100
                    temp_float = _resistance_to_temperature(resistance_mohm, 100);
                    // 12.3 to 123 for HMI
                    temperature[i] = (int32_t)lroundf(temp_float * 10);
                    break;
                case 1: // PT1000
                    temp_float = _resistance_to_temperature(resistance_mohm, 1000);
                    // 12.3 to 123 for HMI
                    temperature[i] = (int32_t)lroundf(temp_float * 10);
                    break;
            }

            // update to modbus table
            if (update_address >= 0) {
                control_logic_update_to_modbus_table(update_address, MODBUS_TYPE_INT32, &temperature[i]);
            }
            // debug(tag, "[port %d] RTD_%d_temp = %.2f, ret = %d", hid_port, i, temp_float[i], ret);
        }
    }

    return ret;
}

int control_hardware_temperature_get_from_ram(uint8_t hid_port, uint8_t channel, int32_t *temp) 
{
    int ret = SUCCESS;

    *temp = 0;

    // channel to rtd address
    uint16_t address =  HID_BASE_ADDRESS + (hid_port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD7124_CH_0_RESISTOR + (channel*2);

    // get modbus param
    modbus_mapping_t *mapping = modbus_manager_data_mapping_get();

    // get resistan
    if (mapping != NULL) {
        AppConvertUint32_16 conv;
        conv.words.u16_byte[0] = mapping->tab_registers[address];
        conv.words.u16_byte[1] = mapping->tab_registers[address+1];
        *temp = conv.val;
    } else {
        ret = FAIL;
    }

    return ret;
}


int control_hardware_temperature_all_get_from_ram(uint8_t hid_port, int32_t temperature[8])
{
    int ret = SUCCESS;

    temperature[0] = 0;
    temperature[1] = 0;
    temperature[2] = 0;
    temperature[3] = 0;
    temperature[4] = 0;
    temperature[5] = 0;
    temperature[6] = 0;
    temperature[7] = 0;

    uint16_t address =  HID_BASE_ADDRESS + (hid_port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_AD7124_CH_0_RESISTOR;

    modbus_mapping_t *mapping = modbus_manager_data_mapping_get();
    if (mapping != NULL) {
        for (int i = 0; i < 8; i++) {
            AppConvertUint32_16 conv;
            conv.words.u16_byte[0] = mapping->tab_registers[address + i*2];
            conv.words.u16_byte[1] = mapping->tab_registers[address + i*2 + 1];
            temperature[i] = conv.val;
        }
    } else {
        ret = FAIL;
    }

    return ret;
}

int control_hardware_resistor_all_get(uint8_t hid_port, uint16_t timeout_ms, uint32_t resistor[8]) 
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;

    // channel to rtd address
    uint16_t rtd_address = DK_MODBUS_AD7124_GET_RESISTANCE_CH_0;

    // get resistance
    ret = CModbusAD7124GetResistance(hid_pid, hid_port, rtd_address, 8, resistor, timeout_ms);

    return ret;
}

int control_hardware_pwm_rpm_get(uint8_t hid_port, uint8_t channel, uint16_t timeout_ms, float *rpm)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;
    uint32_t pwm_period = 0;

    // channel to pwm address
    uint16_t pwm_address = DK_MODBUS_CAP_PWM_FREQ_0 + channel * 2;

    *rpm = 0.0f;

    ret = CModbusCapPWMFrequency(hid_pid, hid_port, pwm_address, 1, &pwm_period, timeout_ms);
    if (ret == SUCCESS) {
        // debug(tag, "[port %d] pwm_%d_period = %d, ret = %d", hid_port, 0, pwm_period, ret);
        
        // us to s
        float period_second = pwm_period / 1000000.0f; 
        
        // period to frequency
        float frequency = 1.0f / period_second; 
        // debug(tag, "[port %d] pwm_%d_freq = %f", hid_port, 0, frequency);
        
        // frequency to rpm
        *rpm = (frequency/2.0f) * 60; 
        // debug(tag, "[port %d] pwm_%d_rpm = %f", hid_port, 0, rpm);
    }

    return ret;
}

int control_hardware_pwm_rpm_get_from_ram(uint8_t hid_port, uint8_t channel, float *rpm)
{
    int ret = SUCCESS;

    *rpm = 0.0f;

    uint32_t pwm_period = 0;

    // get modbus param
    modbus_mapping_t *mapping = modbus_manager_data_mapping_get();

    // get frequency
    if (mapping != NULL) {
        uint16_t target_address = HID_BASE_ADDRESS + (hid_port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_CAPTURE_PWM_0_FREQ + (channel*2);

        AppConvertUint32_16 conv;
        conv.words.u16_byte[0] = mapping->tab_registers[target_address];
        conv.words.u16_byte[1] = mapping->tab_registers[target_address+1];
        pwm_period = conv.val;

        // us to s
        float period_second = pwm_period / 1000000.0f; 
                
        // period to frequency
        float frequency = 1.0f / period_second; 
        // debug(tag, "[port %d] pwm_%d_freq = %f", hid_port, 0, frequency);
        
        // frequency to rpm
        *rpm = (frequency/2.0f) * 60;
        // debug(tag, "[port %d] pwm_%d_rpm = %f", hid_port, 0, rpm);
    } else {
        ret = FAIL;
    }

    return ret;
}

int control_hardware_pwm_period_all_get(uint8_t hid_port, uint32_t period[8])
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;

    uint16_t address = DK_MODBUS_CAP_PWM_PULSE_WIDTH_0;

    ret = CModbusCapPWMPulseWidth(hid_pid, hid_port, address, 8, period);

    return ret;
}

int control_hardware_pwm_freq_all_get(uint8_t hid_port, uint16_t timeout_ms, uint32_t freq[8])
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;

    // channel to pwm address
    uint16_t pwm_address = DK_MODBUS_CAP_PWM_FREQ_0;

    ret = CModbusCapPWMFrequency(hid_pid, hid_port, pwm_address, 8, freq, timeout_ms);

    return ret;
}

int control_hardware_pwm_freq_all_get_from_ram(uint8_t hid_port, uint32_t freq[8])
{
    int ret = SUCCESS;

    freq[0] = 0;
    freq[1] = 0;
    freq[2] = 0;
    freq[3] = 0;
    freq[4] = 0;
    freq[5] = 0;
    freq[6] = 0;
    freq[7] = 0;

    uint16_t address =  HID_BASE_ADDRESS + (hid_port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_CAPTURE_PWM_0_FREQ;

    modbus_mapping_t *mapping = modbus_manager_data_mapping_get();
    if (mapping != NULL) {
        for (int i = 0; i < 8; i++) {
            AppConvertUint32_16 conv;
            conv.words.u16_byte[0] = mapping->tab_registers[address + i*2];
            conv.words.u16_byte[1] = mapping->tab_registers[address + i*2 + 1];
            freq[i] = conv.val;
        }
    } else {
        ret = FAIL;
    }


    return ret;
}

int control_hardware_pwm_duty_set(uint8_t hid_port, uint8_t channel, uint16_t duty)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;

    // channel to pwm address
    uint16_t duty_address = DK_MODBUS_PWM_DUTY_0 + channel;

    // parameter check
    if (duty > 100) {
        error(tag, "duty %d is too high, max is 100", duty);
        duty = 100;
    }

    // set duty
    ret = CModbusPWMOutputSetDuty(hid_pid, hid_port, duty_address, duty);
    // debug(tag, "[port %d] pwm%d_duty = %d, ret = %d", hid_port, channel, duty, ret);

    return ret;
}

int control_hardware_pwm_duty_all_get(uint8_t hid_port, uint16_t timeout_ms, uint16_t duty[8])
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA3;

    // channel to pwm address
    uint16_t address = DK_MODBUS_CAP_PWM_DUTY_0;

    // get duty
    ret = CModbusPWMOutputGetDuty(hid_pid, hid_port, address, 8, duty, timeout_ms);
    // debug(tag, "[port %d] pwm%d_duty = %d, ret = %d", hid_port, channel, duty, ret);

    return ret;
}

int control_hardware_pwm_duty_all_get_from_ram(uint8_t hid_port, uint16_t duty[8])
{
    int ret = SUCCESS;

    duty[0] = 0;
    duty[1] = 0;
    duty[2] = 0;
    duty[3] = 0;
    duty[4] = 0;
    duty[5] = 0;
    duty[6] = 0;
    duty[7] = 0;

    uint16_t address =  HID_BASE_ADDRESS + (hid_port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_CAPTURE_PWM_0_DUTY;

    modbus_mapping_t *mapping = modbus_manager_data_mapping_get();
    if (mapping != NULL) {
        for (int i = 0; i < 8; i++) {
            duty[i] = mapping->tab_registers[address + i];
        }
    } else {
        ret = FAIL;
    }

    return ret;
}

int control_hardware_pwm_period_all_get_from_ram(uint8_t hid_port, uint32_t period[8])
{
    int ret = SUCCESS;

    period[0] = 0;
    period[1] = 0;
    period[2] = 0;
    period[3] = 0;
    period[4] = 0;
    period[5] = 0;
    period[6] = 0;
    period[7] = 0;

    uint16_t address =  HID_BASE_ADDRESS + (hid_port * HID_RTD_BOARD_BASE_ADDRESS) + MODBUS_ADDRESS_CAPTURE_PWM_0_PERIOD;

    modbus_mapping_t *mapping = modbus_manager_data_mapping_get();
    if (mapping != NULL) {
        for (int i = 0; i < 8; i++) {
            AppConvertUint32_16 conv;
            conv.words.u16_byte[0] = mapping->tab_registers[address + i*2];
            conv.words.u16_byte[1] = mapping->tab_registers[address + i*2 + 1];
            period[i] = conv.val;
        }
    } else {
        ret = FAIL;
    }


    return ret;
}

int control_hardware_pwm_freq_set(uint8_t hid_port, uint32_t frequency)
{
    int ret = CModbusPWMOutputSetFrequency(HID_RTD_BOARD_PID, hid_port, frequency);

    return ret;
}

int control_hardware_AI_AO_mode_set(uint8_t hid_port, uint8_t channel, AI_AO_MODE mode, uint16_t timeout_ms)
{
    int ret = SUCCESS;

    uint16_t hid_pid = 0xA2;
    
    uint16_t address = DK_MODBUS_AD74416H_SET_MODE_CH_A + channel;
    
    switch (mode) {
        case AI_AO_MODE_VOLTAGE_OUT:
        case AI_AO_MODE_CURRENT_OUT:
        case AI_AO_MODE_VOLTAGE_IN:
        case AI_AO_MODE_CURRENT_IN_LOOP:
        case AI_AO_MODE_CURRENT_IN_EXTERNAL:
            ret = CModbusAD74416hSetMode(hid_pid, hid_port, address, mode, timeout_ms);
            // debug(tag, "[port %d] ch %d, AI_AO_mode = %d, ret = %d", hid_port, channel, mode, ret);
            break;

        default:
            error(tag, "invalid AI_AO_MODE: %d", mode);
            ret = FAIL;
            break;
    }

    return ret;
}

/**
 * @brief 初始化控制硬體
 *
 * 功能說明:
 * 根據機器類型初始化硬體設備,主要是配置 AI/AO 模式。
 *
 * @param machine_type 機器類型
 *                     - CONTROL_LOGIC_MACHINE_TYPE_LS80: LS80機型
 *                     - CONTROL_LOGIC_MACHINE_TYPE_LX1400: LX1400機型
 *
 * @return int 執行結果
 *         - 0: 初始化成功
 *         - 其他值: 初始化失敗
 *
 * 實現邏輯:
 * LS80 機型配置:
 * - Port 0, CH 0-3: 電流輸入模式(外部供電)
 * - Port 1, CH 0-2: 電流輸入模式(外部供電)
 * - Port 1, CH 3: 電流輸出模式
 *
 * @note 延遲2秒等待硬體穩定
 */
int control_hardware_init(int machine_type)
{
    int ret = 0;

    /* 延遲等待硬體穩定 */
    time_delay_ms(2000);

    switch (machine_type) {
        case CONTROL_LOGIC_MACHINE_TYPE_LS80:
            /* 配置 Port 0 的 AI/AO 模式 */
            ret |= control_hardware_AI_AO_mode_set(0, 0, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            ret |= control_hardware_AI_AO_mode_set(0, 1, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            ret |= control_hardware_AI_AO_mode_set(0, 2, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            ret |= control_hardware_AI_AO_mode_set(0, 3, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            /* 配置 Port 1 的 AI/AO 模式 */
            ret |= control_hardware_AI_AO_mode_set(1, 0, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            ret |= control_hardware_AI_AO_mode_set(1, 1, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            ret |= control_hardware_AI_AO_mode_set(1, 2, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            ret |= control_hardware_AI_AO_mode_set(1, 3, AI_AO_MODE_CURRENT_OUT, 2000);
            debug(tag, "control_hardware_AI_AO_mode_set ret = %d", ret);
            break;

        case CONTROL_LOGIC_MACHINE_TYPE_LX1400:
            /* LX1400 機型配置(待實現) */
            break;

        case CONTROL_LOGIC_MACHINE_TYPE_LS300D:
            /* LS300D 機型配置 - 雙備援感測器設計 */
            /* 配置 Port 0 的 AI/AO 模式 */
            ret |= control_hardware_AI_AO_mode_set(0, 0, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            ret |= control_hardware_AI_AO_mode_set(0, 1, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            ret |= control_hardware_AI_AO_mode_set(0, 2, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            ret |= control_hardware_AI_AO_mode_set(0, 3, AI_AO_MODE_CURRENT_OUT, 2000);
            // /* 配置 Port 1 的 AI/AO 模式 */
            // ret |= control_hardware_AI_AO_mode_set(1, 0, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            // ret |= control_hardware_AI_AO_mode_set(1, 1, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            // ret |= control_hardware_AI_AO_mode_set(1, 2, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
            // ret |= control_hardware_AI_AO_mode_set(1, 3, AI_AO_MODE_CURRENT_OUT, 2000);
            debug(tag, "LS300D control_hardware_AI_AO_mode_set ret = %d", ret);
            break;

        default:
            break;
    }

    return ret;
}
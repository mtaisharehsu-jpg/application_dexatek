#ifndef _DK_MODBUS_AD7124_H_
#define _DK_MODBUS_AD7124_H_

#include <stdint.h>
#include <stdbool.h>
#include "hid_manager.h"
#include "dk_modbus.h"

#define DK_MODBUS_AD7124_GET_RESISTANCE_CH_0   0x1000
#define DK_MODBUS_AD7124_GET_RESISTANCE_CH_1   0x1002
#define DK_MODBUS_AD7124_GET_RESISTANCE_CH_2   0x1004
#define DK_MODBUS_AD7124_GET_RESISTANCE_CH_3   0x1006
#define DK_MODBUS_AD7124_GET_RESISTANCE_CH_4   0x1008
#define DK_MODBUS_AD7124_GET_RESISTANCE_CH_5   0x100A
#define DK_MODBUS_AD7124_GET_RESISTANCE_CH_6   0x100C
#define DK_MODBUS_AD7124_GET_RESISTANCE_CH_7   0x100E

int CModbusAD7124GetResistance(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address,
    uint16_t count, uint32_t* resistance, uint16_t timeout_ms);
int CModbusAD7124GetTemperature(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, uint16_t count, int16_t* temperature );

#endif

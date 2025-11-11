#ifndef _DK_MODBUS_AD74416H_H_
#define _DK_MODBUS_AD74416H_H_

#include <stdint.h>
#include <stdbool.h>
#include "dk_modbus.h"

#define AD74416_MODE_VOLTAGE_OUTPUT         0
#define AD74416_MODE_CURRENT_OUTPUT         1
#define AD74416_MODE_VOLTAGE_INPUT          2
#define AD74416_MODE_CURRENT_INPUT_LOOP     3
#define AD74416_MODE_CURRENT_INPUT_EXT      4

#define DK_MODBUS_AD74416H_SET_MODE_CH_A        0x500
#define DK_MODBUS_AD74416H_SET_MODE_CH_B        0x501
#define DK_MODBUS_AD74416H_SET_MODE_CH_C        0x502
#define DK_MODBUS_AD74416H_SET_MODE_CH_D        0x503

#define DK_MODBUS_AD74416H_VOUT_CLIMIT_CH_A     0x600
#define DK_MODBUS_AD74416H_VOUT_CLIMIT_CH_B     0x602
#define DK_MODBUS_AD74416H_VOUT_CLIMIT_CH_C     0x604
#define DK_MODBUS_AD74416H_VOUT_CLIMIT_CH_D     0x606

#define DK_MODBUS_AD74416H_CIN_CLIMIT_CH_A      0x700
#define DK_MODBUS_AD74416H_CIN_CLIMIT_CH_B      0x702
#define DK_MODBUS_AD74416H_CIN_CLIMIT_CH_C      0x704
#define DK_MODBUS_AD74416H_CIN_CLIMIT_CH_D      0x706

#define DK_MODBUS_AD74416H_VOLTAGE_OUTPUT_CH_A  0x800
#define DK_MODBUS_AD74416H_VOLTAGE_OUTPUT_CH_B  0x802
#define DK_MODBUS_AD74416H_VOLTAGE_OUTPUT_CH_C  0x804
#define DK_MODBUS_AD74416H_VOLTAGE_OUTPUT_CH_D  0x806

#define DK_MODBUS_AD74416H_CURRENT_OUTPUT_CH_A  0x900
#define DK_MODBUS_AD74416H_CURRENT_OUTPUT_CH_B  0x902
#define DK_MODBUS_AD74416H_CURRENT_OUTPUT_CH_C  0x904
#define DK_MODBUS_AD74416H_CURRENT_OUTPUT_CH_D  0x906

#define DK_MODBUS_AD74416H_GET_VOLTAGE_INPUT_CH_A   0xA00
#define DK_MODBUS_AD74416H_GET_VOLTAGE_INPUT_CH_B   0xA02
#define DK_MODBUS_AD74416H_GET_VOLTAGE_INPUT_CH_C   0xA04
#define DK_MODBUS_AD74416H_GET_VOLTAGE_INPUT_CH_D   0xA06

#define DK_MODBUS_AD74416H_GET_CURRENT_INPUT_CH_A   0xB00
#define DK_MODBUS_AD74416H_GET_CURRENT_INPUT_CH_B   0xB02
#define DK_MODBUS_AD74416H_GET_CURRENT_INPUT_CH_C   0xB04
#define DK_MODBUS_AD74416H_GET_CURRENT_INPUT_CH_D   0xB06

int CModbusAD74416hSetMode(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, uint16_t value, uint16_t timeout_ms);
int CModbusAD74416hGetMode(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, uint16_t count, uint16_t* mode, uint16_t timeout_ms);
int CModbusAD74416hGetInput(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, uint16_t count, int32_t* inputValue, uint16_t timeout_ms);
int CModbusAD74416hSetVoutCurLimit(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, int32_t current );
int CModbusAD74416hSetCinCurLimit(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, int32_t current );
int CModbusAD74416hVoltageOutput(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, int32_t voltage, uint16_t timeout_ms);
int CModbusAD74416hCurrentOutput(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, int32_t current, uint16_t timeout_ms);

#endif

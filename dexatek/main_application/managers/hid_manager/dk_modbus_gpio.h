#ifndef _DK_MODBUS_GPIO_H_
#define _DK_MODBUS_GPIO_H_
#include <stdint.h>
#include <stdbool.h>

#define DK_MODBUS_GPIO_OUTPUT_0 0x100
#define DK_MODBUS_GPIO_OUTPUT_1 0x101
#define DK_MODBUS_GPIO_OUTPUT_2 0x102
#define DK_MODBUS_GPIO_OUTPUT_3 0x103
#define DK_MODBUS_GPIO_OUTPUT_4 0x104
#define DK_MODBUS_GPIO_OUTPUT_5 0x105
#define DK_MODBUS_GPIO_OUTPUT_6 0x106
#define DK_MODBUS_GPIO_OUTPUT_7 0x107

#define DK_MODBUS_GPIO_INPUT_0 0x200
#define DK_MODBUS_GPIO_INPUT_1 0x201
#define DK_MODBUS_GPIO_INPUT_2 0x202
#define DK_MODBUS_GPIO_INPUT_3 0x203
#define DK_MODBUS_GPIO_INPUT_4 0x204
#define DK_MODBUS_GPIO_INPUT_5 0x205
#define DK_MODBUS_GPIO_INPUT_6 0x206
#define DK_MODBUS_GPIO_INPUT_7 0x207

int CModbusGPIOOutputPacket( uint8_t* packet, uint16_t address, uint16_t value );
int CModbusGPIOOutputAllPacket( uint8_t* packet, 
        uint16_t s0, uint16_t s1, uint16_t s2, uint16_t s3, 
        uint16_t s4, uint16_t s5, uint16_t s6, uint16_t s7 );

int CModbusGPIOOutput(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, uint16_t value, uint16_t timeout_ms);
int CModbusGPIOOutputAll(uint16_t hid_pid, uint16_t hid_port,
        uint16_t s0, uint16_t s1, uint16_t s2, uint16_t s3, 
        uint16_t s4, uint16_t s5, uint16_t s6, uint16_t s7 );
int CModbusGPIOStatus(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, uint16_t count, uint16_t* status );

#endif


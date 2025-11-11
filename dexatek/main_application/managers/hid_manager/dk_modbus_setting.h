#ifndef _DK_MODBUS_SETTING_H_
#define _DK_MODBUS_SETTING_H_

#include <stdint.h>
#include <stdbool.h>

#define DK_MODBUS_UART_SETTING_DATA_BITS_8  8
#define DK_MODBUS_UART_SETTING_DATA_BITS_7  7

#define DK_MODBUS_UART_SETTING_PARITY_NONE  0
#define DK_MODBUS_UART_SETTING_PARITY_ODD   1
#define DK_MODBUS_UART_SETTING_PARITY_EVEN  2

#define DK_MODBUS_UART_SETTING_STOP_BITS_1  1
#define DK_MODBUS_UART_SETTING_STOP_BITS_2  2

int CModbusUartSettingPacket( uint8_t* packet, int baudrate, uint16_t databits, uint16_t parity, uint16_t stopbits );
int CModbusUartBaudratePacket( uint8_t* packet, int baudrate );
int CModbusUartDatabitsPacket( uint8_t* packet, uint16_t databits );
int CModbusUartParityPacket( uint8_t* packet, uint16_t parity );
int CModbusUartStopbitsPacket( uint8_t* packet, uint16_t stopbits );

int CModbusUartBaudrate(uint16_t hid_pid, uint16_t hid_port, int baudrate );
int CModbusUartDatabits(uint16_t hid_pid, uint16_t hid_port, uint16_t databits );
int CModbusUartParity(uint16_t hid_pid, uint16_t hid_port, uint16_t parity );
int CModbusUartStopbits(uint16_t hid_pid, uint16_t hid_port, uint16_t stopbits );

#endif

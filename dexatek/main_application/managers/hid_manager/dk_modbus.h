#ifndef _MODBUS_H_
#define _MODBUS_H_

#include <stdint.h>
#include <stdbool.h>
#define DK_MODBUS_DEVICE_ID 240

#define MODBUS_ERROR_CODE_FC_ERROR      1
#define MODBUS_ERROR_CODE_ADDR_ERROR    2
#define MODBUS_ERROR_CODE_VALUE_ERROR   3
#define MODBUS_ERROR_CODE_DEVICE_ERROR  4
#define MODBUS_ERROR_CODE_ACK_ERROR     5
#define MODBUS_ERROR_CODE_BUSY_ERROR    6


int CModbusReadPacket( uint8_t* packet, uint8_t id, uint16_t address, uint16_t count );
int CModbusReadPacketEx( uint8_t* packet, uint8_t id, uint8_t function_code, uint16_t address, uint16_t count );
bool CModbusCheckReadPacket( uint8_t* packet );
int CModbusResponseReadPacket( uint8_t* packet, uint8_t id, uint8_t* data, uint8_t length );
bool CModbusCheckResponseReadPacket( uint8_t* packet);
int CModbusWritePacket( uint8_t* packet, uint8_t id, uint16_t address, uint16_t value );
bool CModbusCheckWritePacket( uint8_t* packet );
int CModbusWriteMultiplePacket( uint8_t* packet, uint8_t id, uint16_t address, uint8_t* data, uint8_t length  );
bool CModbusCheckWriteMultiplePacket( uint8_t* packet );
int CModbusResponseWriteMultiplePacket( uint8_t* packet, uint8_t id, uint16_t address, uint8_t length  );
bool CModbusCheckResponseWriteMultiplePacket( uint8_t* packet );
int CModbusErrorResponsePacket( uint8_t* packet, uint8_t id, uint8_t functionCode, uint8_t errorCode );
bool CModbusCheckErrorResponsePacket( uint8_t* packet );

uint8_t CModbusID( uint8_t* packet );
uint8_t CModbusFC( uint8_t* packet );
uint16_t CModbusAddress( uint8_t* packet );
uint16_t CModbusValue( uint8_t* packet );
uint16_t CModbusCount( uint8_t* packet );
uint8_t* CModbusWriteContent( uint8_t* packet );
uint8_t CModbusWriteLength( uint8_t* packet );
uint8_t* CModbusReadContent( uint8_t* packet );
uint8_t CModbusReadLength( uint8_t* packet );
uint8_t CModbusErrorCode( uint8_t* packet );

#endif

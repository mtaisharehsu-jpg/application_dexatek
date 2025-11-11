#ifndef _DK_MODBUS_PWM_H_
#define _DK_MODBUS_PWM_H_
#include <stdint.h>
#include <stdbool.h>

#define DK_MODBUS_PWM_FREQUENCY 0x3E0

#define DK_MODBUS_PWM_DUTY_0    0x300
#define DK_MODBUS_PWM_DUTY_1    0x301
#define DK_MODBUS_PWM_DUTY_2    0x302
#define DK_MODBUS_PWM_DUTY_3    0x303
#define DK_MODBUS_PWM_DUTY_4    0x304
#define DK_MODBUS_PWM_DUTY_5    0x305
#define DK_MODBUS_PWM_DUTY_6    0x306
#define DK_MODBUS_PWM_DUTY_7    0x307

int CModbusPWMOutputFrequencyPacket( uint8_t* packet, uint32_t frequency );
int CModbusPWMOutputPacket( uint8_t* packet, uint16_t address, uint16_t value );
int CModbusPWMOutputAllPacket( uint8_t* packet, 
        uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, 
        uint16_t d4, uint16_t d5, uint16_t d6, uint16_t d7 );

int CModbusPWMOutputSetFrequency(uint16_t hid_pid, uint16_t hid_port, uint32_t frequency );
int CModbusPWMOutputGetFrequency(uint16_t hid_pid, uint16_t hid_port, uint8_t* frequency );
int CModbusPWMOutputSetDuty(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, uint16_t duty );
int CModbusPWMOutputSetAllDuty(uint16_t hid_pid, uint16_t hid_port,
        uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, 
        uint16_t d4, uint16_t d5, uint16_t d6, uint16_t d7);
int CModbusPWMOutputGetDuty(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address,
         uint16_t count, uint16_t* duty, uint16_t timeout_ms);

#endif


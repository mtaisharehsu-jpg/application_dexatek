#ifndef _DK_MODBUS_CAP_PWM_H_
#define _DK_MODBUS_CAP_PWM_H_
#include <stdint.h>
#include <stdbool.h>

#define DK_MODBUS_CAP_PWM_DUTY_0    0x400
#define DK_MODBUS_CAP_PWM_DUTY_1    0x401
#define DK_MODBUS_CAP_PWM_DUTY_2    0x402
#define DK_MODBUS_CAP_PWM_DUTY_3    0x403
#define DK_MODBUS_CAP_PWM_DUTY_4    0x404
#define DK_MODBUS_CAP_PWM_DUTY_5    0x405
#define DK_MODBUS_CAP_PWM_DUTY_6    0x406
#define DK_MODBUS_CAP_PWM_DUTY_7    0x407

#define DK_MODBUS_CAP_PWM_FREQ_0    0x410
#define DK_MODBUS_CAP_PWM_FREQ_1    0x412
#define DK_MODBUS_CAP_PWM_FREQ_2    0x414
#define DK_MODBUS_CAP_PWM_FREQ_3    0x416
#define DK_MODBUS_CAP_PWM_FREQ_4    0x418
#define DK_MODBUS_CAP_PWM_FREQ_5    0x41A
#define DK_MODBUS_CAP_PWM_FREQ_6    0x41C
#define DK_MODBUS_CAP_PWM_FREQ_7    0x41E

#define DK_MODBUS_CAP_PWM_PULSE_WIDTH_0    0x420
#define DK_MODBUS_CAP_PWM_PULSE_WIDTH_1    0x422
#define DK_MODBUS_CAP_PWM_PULSE_WIDTH_2    0x424
#define DK_MODBUS_CAP_PWM_PULSE_WIDTH_3    0x426
#define DK_MODBUS_CAP_PWM_PULSE_WIDTH_4    0x428
#define DK_MODBUS_CAP_PWM_PULSE_WIDTH_5    0x42A
#define DK_MODBUS_CAP_PWM_PULSE_WIDTH_6    0x42C
#define DK_MODBUS_CAP_PWM_PULSE_WIDTH_7    0x42E


int CModbusCapPWMDuty(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, uint16_t count, uint16_t* duty );
int CModbusCapPWMFrequency(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, 
                           uint16_t count, uint32_t* frequency, uint16_t timeout_ms);
int CModbusCapPWMPulseWidth(uint16_t hid_pid, uint16_t hid_port, uint16_t hid_address, uint16_t count, uint32_t* pulseWidth );

#endif

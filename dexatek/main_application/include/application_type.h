#ifndef APPLICATION_TYPE_H
#define APPLICATION_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// types
#ifndef SUCCESS
#	define		SUCCESS				0
#endif

#ifndef FAIL
#	define		FAIL			   -1
#endif

#ifndef BOOL
#	define		BOOL				uint8_t
#endif

#ifndef TRUE
#	define		TRUE				1
#endif

#ifndef FALSE
#	define		FALSE				0
#endif

#ifndef INT32_INVALID
#   define INT32_INVALID           INT_MIN
#endif

// convert
typedef union {
    uint16_t val;
    struct {
        uint8_t byte[2];
    } bytes;
} AppConvert16_8;

typedef union {
    uint32_t val;
    struct {
        uint8_t byte[4];
    } bytes;
} AppConvert32_8;

typedef union {
    float fVal;
    struct {
        uint16_t u16_byte[2];
    } bytes;
} AppConvertFloat32_16;

typedef union {
    uint32_t val;
    struct {
        uint16_t u16_byte[2];
    } words;
} AppConvertUint32_16;


typedef union {
    int val;
    struct {
        uint16_t u16_byte[2];
    } words;
} AppConvertInt32_16;

typedef union {
    uint64_t val;
    struct {
        uint16_t u16_byte[4];
    } words;
} AppConvert64_16;

typedef union {
    uint64_t val;
    struct {
        uint8_t byte[8];
    } bytes;
} AppConvert64_8;

typedef union {
    uint8_t data;
    struct {
        uint8_t bit0:1;
        uint8_t bit1:1;
        uint8_t bit2:1;
        uint8_t bit3:1;
        uint8_t bit4:1;
        uint8_t bit5:1;
        uint8_t bit6:1;
        uint8_t bit7:1;
    } bits;
} AppBitwise_8;

typedef union {
    uint16_t data;
    struct {
        uint8_t bit0:1;
        uint8_t bit1:1;
        uint8_t bit2:1;
        uint8_t bit3:1;
        uint8_t bit4:1;
        uint8_t bit5:1;
        uint8_t bit6:1;
        uint8_t bit7:1;
        uint8_t bit8:1;
        uint8_t bit9:1;
        uint8_t bit10:1;
        uint8_t bit11:1;
        uint8_t bit12:1;
        uint8_t bit13:1;
        uint8_t bit14:1;
        uint8_t bit15:1;
    } bits;
} AppBitwise_16;

#ifdef __cplusplus
}
#endif

#endif
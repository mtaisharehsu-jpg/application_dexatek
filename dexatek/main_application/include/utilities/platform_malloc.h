#ifndef PLATFORM_MALLOC_H
#define PLATFORM_MALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#define PLATFORM_MALLOC_DBG CONFIG_PLATFORM_MALLOC_DBG

void platform_memory_init(void);
void platform_memory_report(void);

#if defined(PLATFORM_MALLOC_DBG) && PLATFORM_MALLOC_DBG

#define platform_slow_malloc(size) platform_slow_malloc_dbg(size, __FUNCTION__, __LINE__)
#define platform_slow_calloc(num, size) platform_slow_calloc_dbg(num, size, __FUNCTION__, __LINE__)
#define platform_fast_calloc(num, size) platform_fast_calloc_dbg(num, size, __FUNCTION__, __LINE__)

void* platform_slow_malloc_dbg(int size, const char* strFunc, uint32_t line);
void* platform_fast_malloc_dbg(int size, const char* strFunc, uint32_t line);
void* platform_slow_calloc_dbg(int number, int size, const char* strFunc, uint32_t line);
void* platform_fast_calloc_dbg(int number, int size, const char* strFunc, uint32_t line);

void platform_slow_free(void* mem);
void platform_fast_free(void *mem);

#else

void* platform_slow_malloc(int size);
void* platform_slow_calloc(int num, int size);

void* platform_fast_malloc(int size);
void* platform_fast_calloc(int number, int size);

void platform_slow_free(void* mem);
void platform_fast_free(void *mem);
#endif


#ifdef __cplusplus
}
#endif

#endif
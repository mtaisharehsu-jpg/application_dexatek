
#ifndef PLATFORM_TASK_H
#define PLATFORM_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void * (*PlatformTaskCuntion)(void *);
typedef void * PlatformTaskHandle;

typedef void * TaskReturn;

int platform_task_create(PlatformTaskCuntion task_function,
                        char* name,
		                uint32_t stack_size,
		                void* const parameter, 
		                unsigned long priority, 
                        PlatformTaskHandle* handle);

int platform_task_cancel(PlatformTaskHandle handle);

#ifdef __cplusplus
}
#endif

#endif
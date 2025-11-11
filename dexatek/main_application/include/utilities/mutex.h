
#ifndef MUTEX_H
#define MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MUTEX_TAKE_WAIT_FOREVER 0xFFFFFFFF

typedef void* MutexContext;

MutexContext mutex_create(void);
int mutex_take(MutexContext mutex, uint32_t xBlockTimeInMS);
int mutex_give(MutexContext mutex);
int mutex_delete(MutexContext mutex);


#ifdef __cplusplus
}
#endif

#endif
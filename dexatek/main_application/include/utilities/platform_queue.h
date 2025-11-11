
#ifndef PLATFORM_QUEUE_H
#define PLATFORM_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

#include "dexatek/main_application/include/utilities/list.h"
#include "dexatek/main_application/include/utilities/mutex.h"

typedef struct platform_queue_handle_t {
    int32_t size;
    int32_t max_size;
    struct list_head queue_list;
    MutexContext mutex;
} PlatformQueueHandle;


PlatformQueueHandle* platform_queue_create(int32_t size);

int platform_queue_destroy(PlatformQueueHandle* queue);

BOOL platform_queue_is_full(PlatformQueueHandle* queue);

int platform_queue_enqueue(PlatformQueueHandle* queue, void* item);

int platform_queue_dequeue(PlatformQueueHandle* queue, void** item);

int platform_queue_peek(PlatformQueueHandle* queue, void** item);


void platform_queue_dump(PlatformQueueHandle* queue);

#ifdef __cplusplus
}
#endif

#endif
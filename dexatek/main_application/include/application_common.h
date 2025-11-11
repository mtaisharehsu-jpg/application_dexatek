#ifndef APPLICATION_COMMON_H
#define APPLICATION_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>

#include "dexatek/main_application/project_config.h"

#include "dexatek/main_application/include/application_log.h"
#include "dexatek/main_application/include/application_type.h"

#include "dexatek/main_application/include/utilities/platform_malloc.h"
#include "dexatek/main_application/include/utilities/platform_task.h"
#include "dexatek/main_application/include/utilities/platform_queue.h"
#include "dexatek/main_application/include/utilities/os_utilities.h"
#include "dexatek/main_application/include/utilities/mutex.h"

#ifdef __cplusplus
}
#endif

#endif

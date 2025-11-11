// Stub implementations for missing external functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// Platform-specific stubs
typedef struct {
    int dummy;
} MutexContext_t;
typedef MutexContext_t* MutexContext;

typedef struct {
    int dummy;
} PlatformTaskHandle_t;
typedef PlatformTaskHandle_t* PlatformTaskHandle;

typedef void* TaskReturn;

// Mutex stubs
MutexContext mutex_create(void) {
    static MutexContext_t ctx = {0};
    return &ctx;
}

void mutex_delete(MutexContext ctx) {
    (void)ctx; // Unused
}

// Platform task stubs
int platform_task_create(TaskReturn (*func)(void*), const char* name, size_t stack_size, void* param, int priority, PlatformTaskHandle* handle) {
    (void)func; (void)name; (void)stack_size; (void)param; (void)priority;
    static PlatformTaskHandle_t task = {0};
    *handle = &task;
    
    // Create a detached thread
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    if (pthread_create(&thread, &attr, (void*(*)(void*))func, param) != 0) {
        pthread_attr_destroy(&attr);
        return -1;
    }
    
    pthread_attr_destroy(&attr);
    return 0;
}

void platform_task_cancel(PlatformTaskHandle handle) {
    (void)handle; // Unused - can't easily cancel a detached thread
}

// Network utility stubs
int net_mac_get(const char* iface, unsigned char* mac) {
    (void)iface;
    // Fake MAC address
    mac[0] = 0x02; mac[1] = 0x42; mac[2] = 0xAC;
    mac[3] = 0x11; mac[4] = 0x00; mac[5] = 0x02;
    return 0;
}

int net_config_is_dhcp(void) {
    return 1; // Assume DHCP
}

int net_ethernet_config_restart(void) {
    return 0; // Success
}

// Time utilities
void time_get_current_date_string_r(char* buffer, size_t size) {
    time_t rawtime;
    struct tm* timeinfo;
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", timeinfo);
}

// mDNS service stubs
typedef struct {
    char name[256];
    char reg_type[64];
    int port;
    void* txtRecord;
} MdnsServiceConfig;

int mdns_service_run(MdnsServiceConfig* config) {
    printf("mDNS service started: %s on port %d\n", config->name, config->port);
    return 0;
}

int mdns_service_stop(MdnsServiceConfig* config) {
    printf("mDNS service stopped: %s\n", config->name);
    return 0;
}

// TXT Record stubs (DNS Service Discovery)
typedef struct {
    char data[512];
    int len;
} TXTRecordRef;

void TXTRecordCreate(TXTRecordRef* txtRecord, int bufferLen, void* buffer) {
    (void)bufferLen; (void)buffer;
    txtRecord->len = 0;
    memset(txtRecord->data, 0, sizeof(txtRecord->data));
}

int TXTRecordSetValue(TXTRecordRef* txtRecord, const char* key, int valueLen, const void* value) {
    (void)txtRecord; (void)key; (void)valueLen; (void)value;
    return 0; // Success
}

// Debug/logging stubs
void debug(const char* tag, const char* format, ...) {
    (void)tag; (void)format;
    // Silent debug
}

void info(const char* tag, const char* format, ...) {
    (void)tag; (void)format;
    // Silent info
}

void error(const char* tag, const char* format, ...) {
    (void)tag; (void)format;
    // Silent error
}

// Boolean type if not available
#ifndef BOOL
#define BOOL int
#define TRUE 1
#define FALSE 0
#endif

#ifndef REDFISH_HID_BRIDGE_H
#define REDFISH_HID_BRIDGE_H

#include "config.h"
#include <stdint.h>
#include <stddef.h>

// Simple HID bridge interface for Redfish OEM integrations
// This module provides lightweight, optionally-stubbed hooks to enumerate
// and interact with HID devices exposed by the platform (e.g., /dev/hidraw*).

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the HID bridge subsystem
int redfish_hid_init(void);

// Deinitialize the HID bridge subsystem
void redfish_hid_deinit(void);

// Returns 1 if the HID subsystem is available on this platform, 0 otherwise
int redfish_hid_is_available(void);

// Get a list of detected HID device PIDs into hid_pid_list
int redfish_hid_device_list_get(uint16_t* hid_pid_list_buffer, size_t hid_pid_list_buffer_size, size_t* hid_pid_list_size);

// Open the specified HID device PID
int redfish_hid_open(uint16_t hid_pid);

// Close the currently opened HID device (if any)
void redfish_hid_close(void);

// Write a raw HID report to the opened device
// int redfish_hid_write(uint16_t hid_pid, uint16_t hid_port, const uint8_t *write_buffer, size_t write_buffer_size, int timeout_ms);

// Read a raw HID report from the opened device
// timeout_ms <= 0 means non-blocking/instant return
// int redfish_hid_read(uint16_t hid_pid, uint16_t hid_port, uint8_t *buffer, size_t buffer_size, size_t* bytes_read, int timeout_ms);

int redfish_board_data_append_to_json(uint16_t port_idx, cJSON *json_root);
int redfish_board_write(uint16_t port_idx, const char *jsonPayload, uint16_t timeout_ms);

int redfish_control_logic_data_append_to_json(uint16_t control_logic_idx, cJSON *json_root);
int redfish_control_logic_write(uint16_t control_logic_idx, const char *jsonPayload, uint16_t timeout_ms);


#ifdef __cplusplus
}
#endif

#endif // REDFISH_HID_BRIDGE_H


#ifndef HID_MANAGER_H
#define HID_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#define HID_DEVICES_MAX 4

#define HID_READ_TIMEOUT_MS 2000
#define HID_WRITE_TIMEOUT_MS 2000

typedef enum {
    HID_HUBS_DEPTH_0 = 3,
    HID_HUBS_DEPTH_1 = 4,
    HID_HUBS_DEPTH_2 = 5,
    HID_HUBS_DEPTH_3 = 6,
} HID_HUB_HIERARCHY;

typedef enum {
    HID_IO_BOARD_PID = 0xA2,
    HID_RTD_BOARD_PID = 0xA3,
} HID_BOARD_PID;

int hid_manager_init(void);
int hid_manager_deinit(void);

int hid_manager_write(uint16_t hid_pid, uint16_t hid_port, uint8_t *data, size_t length, int timeout_ms);
int hid_manager_read(uint16_t hid_pid, uint16_t hid_port, uint8_t *data, size_t length, int timeout_ms);

int hid_manager_device_vid_get(uint16_t hid_pid, uint16_t hid_port, uint16_t *vid);
int hid_manager_device_pid_get(uint16_t hid_pid, uint16_t hid_port, uint16_t *pid);

int hid_manager_port_pid_get(uint16_t port, uint16_t *pid);

int hid_manager_reset_usb_hub(void);

#ifdef __cplusplus
}
#endif

#endif

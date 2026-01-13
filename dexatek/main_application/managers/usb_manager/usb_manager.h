#ifndef USB_MANAGER_H
#define USB_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int usb_manager_init(void);
int usb_manager_deinit(void);

BOOL usb_manager_storage_mounted(void);
int usb_manager_fsck_vfat(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* USB_MANAGER_H */

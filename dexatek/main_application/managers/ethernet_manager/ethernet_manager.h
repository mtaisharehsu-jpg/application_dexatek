#ifndef ETHERNET_MANAGER_H
#define ETHERNET_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int ethernet_manager_restore_config_to_backup(void);

int ethernet_manager_restart(void);
int ethernet_manager_init(void);
int ethernet_manager_deinit(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ETHERNET_MANAGER_H */

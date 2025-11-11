#ifndef REDFISH_INIT_H
#define REDFISH_INIT_H


int redfish_init(void);
int redfish_deinit(void);
int handle_client_connection(int client_fd);


#endif
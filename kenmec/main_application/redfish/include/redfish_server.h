#ifndef REDFISH_SERVER_H
#define REDFISH_SERVER_H

#include "../include/config.h"

// Redfish resource types
typedef enum {
    REDFISH_RESOURCE_VERSION,
    REDFISH_RESOURCE_SERVICE_ROOT,                      // Unused
    REDFISH_RESOURCE_ODATA_SERVICE,                     // /redfish/v1/odata
    REDFISH_RESOURCE_ODATA_METADATA,                    // /redfish/v1/$metadata
    REDFISH_RESOURCE_CHASSIS_COLLECTION,                // Unused
    REDFISH_RESOURCE_CHASSIS,                           // Unused
    REDFISH_RESOURCE_SYSTEMS_COLLECTION,                // Unused
    REDFISH_RESOURCE_SYSTEM,                            // Unused
    REDFISH_RESOURCE_MANAGERS_ETHERNET_INTERFACE_ETH0,
    REDFISH_RESOURCE_MANAGERS_ETHERNET_INTERFACE,
    REDFISH_RESOURCE_MANAGERS_KENMEC,
    REDFISH_RESOURCE_MANAGERS_COLLECTION,
    REDFISH_RESOURCE_MANAGER,
    REDFISH_RESOURCE_MANAGER_SECURITY_POLICY,
    REDFISH_RESOURCE_MANAGER_NETWORK_PROTOCOL,
    REDFISH_RESOURCE_MANAGER_NETWORK_PROTOCOL_HTTPS_CERTIFICATES,
    REDFISH_RESOURCE_MANAGER_NETWORK_PROTOCOL_HTTPS_CERTIFICATE,
    REDFISH_RESOURCE_MANAGER_RESET_ACTION,
    REDFISH_RESOURCE_SESSIONSERVICE,
    REDFISH_RESOURCE_SESSIONSERVICE_SESSIONS,
    REDFISH_RESOURCE_SESSIONSERVICE_SESSIONS_MEMBERS,
    REDFISH_RESOURCE_ACCOUNTSERVICE_ACCOUNTS,
    REDFISH_RESOURCE_ACCOUNTSERVICE_ACCOUNT,
    REDFISH_RESOURCE_ACCOUNTSERVICE,
    REDFISH_RESOURCE_ACCOUNTSERVICE_ROLES_COLLECTION,
    REDFISH_RESOURCE_ACCOUNTSERVICE_ROLE,
	REDFISH_RESOURCE_CERTIFICATESERVICE,
	REDFISH_RESOURCE_CERTIFICATESERVICE_GENERATE_CSR,
	REDFISH_RESOURCE_CERTIFICATESERVICE_REPLACE_CERTIFICATE,
	REDFISH_RESOURCE_MANAGER_SECURITY_POLICY_TRUSTED_CERTIFICATES,
	REDFISH_RESOURCE_MANAGER_SECURITY_POLICY_TRUSTED_CERTIFICATE,
    REDFISH_RESOURCE_UPDATESERVICE,
    REDFISH_RESOURCE_UPDATESERVICE_MULTIPART,
    REDFISH_RESOURCE_THERMALEQUIPMENT_COLLECTION,
    REDFISH_RESOURCE_THERMALEQUIPMENT,
    REDFISH_RESOURCE_CDU_OEM,
    REDFISH_RESOURCE_CDU_OEM_KENMEC,
    REDFISH_RESOURCE_CDU_OEM_IOBOARDS,
    REDFISH_RESOURCE_CDU_OEM_IOBOARD_MEMBER,
    REDFISH_RESOURCE_CDU_OEM_IOBOARD_ACTION_READ,
    REDFISH_RESOURCE_CDU_OEM_IOBOARD_ACTION_WRITE,
    REDFISH_RESOURCE_CDU_OEM_KENMEC_CONFIG_READ,
    REDFISH_RESOURCE_CDU_OEM_KENMEC_CONFIG_WRITE,
    REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS,
    REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS_MEMBER,
    REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS_ACTION_READ,
    REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS_ACTION_WRITE,
    REDFISH_RESOURCE_UNKNOWN
} redfish_resource_type_t;

// Label post action types - actions to be performed after response is sent
typedef enum {
    LABEL_POST_ACTION_NONE = 0,
    LABEL_POST_ACTION_FORCE_RESTART,
    // Add more post actions here as needed
    // LABEL_POST_ACTION_SHUTDOWN,
    // LABEL_POST_ACTION_REBOOT,
    // LABEL_POST_ACTION_MAINTENANCE_MODE,
} label_post_action_t;

// HTTP request structure
typedef struct {
    char method[16];
    char path[MAX_PATH_LENGTH];
    char headers[MAX_HEADERS][2][256];
    int header_count;
    char body[MAX_JSON_SIZE];
    int content_length;
    int is_https;  // 1 if HTTPS, 0 if HTTP
    // For large uploads, the body can be streamed to a file
    char upload_tmp_path[256];
} http_request_t;

// HTTP response structure
typedef struct {
    int status_code;
    char content_type[64];
    char body[MAX_JSON_SIZE];
    int content_length;
    char headers[MAX_HEADERS][2][MAX_HEADER_VALUE_LEN];
    int header_count;
    label_post_action_t post_action;
} http_response_t;

// Function declarations
int redfish_server_init(void);
void redfish_server_cleanup(void);
int parse_http_request(const char *raw_request, http_request_t *request, int is_https);
int process_redfish_request(const http_request_t *request, http_response_t *response);
void generate_http_response(const http_response_t *response, char *output, size_t output_size);

// HTTP/HTTPS server functions
int handle_http_client_connection(int client_fd);
int handle_https_client_connection(int client_fd);
int http_server_init(int port);
int http_server_get_fd(void);
void http_server_cleanup(void);
int https_server_init(int port, const char *cert_file, const char *key_file, const char *client_ca_file);
void https_server_cleanup(void);

// Redfish resource handlers
int handle_service_root(http_response_t *response);
int handle_chassis_collection(http_response_t *response);
int handle_chassis(const char *chassis_id, http_response_t *response);
int handle_systems_collection(http_response_t *response);
int handle_system(const char *system_id, http_response_t *response);
int handle_manager(const char *manager_id, http_response_t *response);
int handle_thermalequipment_collection(http_response_t *response);
int handle_thermalequipment(const char *thermalequipment_id, http_response_t *response);
int handle_cdu_oem_ioboards(const char *cdu_id, http_response_t *response);


// POST handlers (create new resources)
int handle_chassis_create(const http_request_t *request, http_response_t *response);
int handle_system_create(const http_request_t *request, http_response_t *response);
int handle_manager_create(const http_request_t *request, http_response_t *response);
int handle_thermalequipment_create(const http_request_t *request, http_response_t *response);
int handle_accounts_create(const http_request_t *request, http_response_t *response);
int handle_sessions(const http_request_t *request, http_response_t *response);
int handle_session_delete(const char *token, http_response_t *response);
int handle_sessions_collection(const http_request_t *request, http_response_t *response);
int handle_session_member(const char *session_id, http_response_t *response);
int handle_accounts(void);

// PUT handlers (update existing resources)
int handle_chassis_update(const char *chassis_id, const http_request_t *request, http_response_t *response);
int handle_system_update(const char *system_id, const http_request_t *request, http_response_t *response);
int handle_manager_update(const char *manager_id, const http_request_t *request, http_response_t *response);
int handle_thermalequipment_update(const char *thermalequipment_id, const http_request_t *request, http_response_t *response);

// DELETE handlers (remove resources)
int handle_chassis_delete(const char *chassis_id, http_response_t *response);
int handle_system_delete(const char *system_id, http_response_t *response);
int handle_manager_delete(const char *manager_id, http_response_t *response);
int handle_thermalequipment_delete(const char *thermalequipment_id, http_response_t *response);

// Utility functions
redfish_resource_type_t parse_redfish_path(const char *path, char **resource_id);
const char* get_http_status_text(int status_code);
void add_cors_headers(http_response_t *response);

// Post action functions
int redfish_server_post_action(label_post_action_t action, const char *context);

#endif // REDFISH_SERVER_H
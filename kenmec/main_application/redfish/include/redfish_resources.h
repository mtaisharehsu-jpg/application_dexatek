#ifndef REDFISH_RESOURCES_H
#define REDFISH_RESOURCES_H

#include "redfish_server.h"
#include "redfish_crypto.h"


typedef enum {
    MANAGER_NONE,
    MANAGER_COLLECTION,
    MANAGER_KENMEC,
    MANAGER_ETHERNET_INTERFACE,
    MANAGER_ETHERNET_INTERFACE_ETH0
} json_resource_type_t;


// Function declarations for Redfish resource handlers
int handle_service_root(http_response_t *response);
int handle_version_root(http_response_t *response);
int handle_odata_service(http_response_t *response);
int handle_odata_metadata(http_response_t *response);
int handle_account_service(http_response_t *response);
int handle_accounts_collection(http_response_t *response);
int handle_account_member(const char *account_id, http_response_t *response);
int handle_account_patch(const char *account_id, const http_request_t *request, http_response_t *response);
int handle_chassis_collection(http_response_t *response);
int handle_chassis(const char *chassis_id, http_response_t *response);
int handle_systems_collection(http_response_t *response);
int handle_system(const char *system_id, http_response_t *response);
int get_managers_ethernet_interface_eth0(const char *resource_id, http_response_t *response);
int get_managers_ethernet_interface(const char *resource_id, http_response_t *response);
int get_managers_kenmec_resource(const char *resource_id, http_response_t *response);
int get_managers_collection(const char *resource_id, http_response_t *response);
int get_manager_security_policy(const char *manager_id, http_response_t *response);
int post_manager_security_policy(const char *manager_id, const http_request_t *request, http_response_t *response);
int patch_manager_security_policy(const char *manager_id, const http_request_t *request, http_response_t *response);
int delete_manager_security_policy(const char *manager_id, http_response_t *response);
int handle_manager_network_protocol(const char *manager_id, http_response_t *response);
int handle_manager_network_protocol_https_certificates(const char *manager_id, http_response_t *response);
int handle_manager_network_protocol_https_certificate_member(const char *manager_id, const char *cert_id, http_response_t *response);
int handle_manager(const char *manager_id, http_response_t *response);
int handle_session_service(http_response_t *response);
int handle_sessions_collection(const http_request_t *request, http_response_t *response);
int handle_session_member(const char *session_id, http_response_t *response);
int handle_roles_collection(http_response_t *response);
int handle_role_member(const char *role_id, http_response_t *response);
int handle_thermalequipment_collection(http_response_t *response);
int handle_thermalequipment(const char *resource_id, http_response_t *response);

int handle_cdu_oem(const char *cdu_id, http_response_t *response);
int handle_cdu_oem_kenmec(const char *cdu_id, http_response_t *response);
int handle_cdu_oem_ioboards(const char *cdu_id, http_response_t *response);
int handle_cdu_oem_ioboard_member(const char *cdu_id, const char *member_id, http_response_t *response);
int handle_cdu_oem_ioboard_action_read(const char *cdu_id, const char *member_id, const http_request_t *request, http_response_t *response);
int handle_cdu_oem_ioboard_action_write(const char *cdu_id, const char *member_id, const http_request_t *request, http_response_t *response);
int handle_cdu_oem_config(const char *cdu_id, http_response_t *response);
int handle_cdu_oem_kenmec_config_write(const char *cdu_id, http_request_t *request, http_response_t *response);
int handle_cdu_oem_kenmec_config_read(const char *cdu_id, http_response_t *response);

int handle_cdu_oem_control_logics(const char *cdu_id, http_response_t *response);
int handle_cdu_oem_control_logics_member(const char *cdu_id, const char *member_id, http_response_t *response);
int handle_cdu_oem_control_logics_action_read(const char *cdu_id, const char *member_id, const http_request_t *request, http_response_t *response);
int handle_cdu_oem_control_logics_action_write(const char *cdu_id, const char *member_id, const http_request_t *request, http_response_t *response);

int handle_manager_reset_action(const char *manager_id, const http_request_t *request, http_response_t *response);

int handle_certificate_service(http_response_t *response);
int handle_certificate_service_generate_csr(const http_request_t *request, http_response_t *response);
int handle_certificate_service_replace_certificate(const http_request_t *request, http_response_t *response);
int handle_manager_security_policy_trusted_certificates(http_response_t *response);
int handle_manager_security_policy_trusted_certificate(const char *cert_id, http_response_t *response);
int handle_update_service(http_response_t *response);
int handle_update_service_multipart_upload(const http_request_t *request, http_response_t *response);

// Certificate parsing function
int redfish_parse_certificate_info(const char *pem_cert,
    char *issuer_country, size_t issuer_country_size,
    char *issuer_state, size_t issuer_state_size,
    char *issuer_city, size_t issuer_city_size,
    char *issuer_org, size_t issuer_org_size,
    char *issuer_ou, size_t issuer_ou_size,
    char *issuer_cn, size_t issuer_cn_size,
    char *subject_country, size_t subject_country_size,
    char *subject_state, size_t subject_state_size,
    char *subject_city, size_t subject_city_size,
    char *subject_org, size_t subject_org_size,
    char *subject_ou, size_t subject_ou_size,
    char *subject_cn, size_t subject_cn_size,
    char *valid_not_before, size_t valid_not_before_size,
    char *valid_not_after, size_t valid_not_after_size);

int patch_managers_ethernet_interface_eth0(const char *resource_id, const http_request_t *request, http_response_t *response);


// Utility functions for JSON generation
char* generate_service_root_json(void);
char* generate_chassis_collection_json(void);
char* generate_chassis_json(const char *chassis_id);
char* generate_systems_collection_json(void);
char* generate_system_json(const char *system_id);
char* generate_managers_ethernet_interface_json(void);
char* generate_managers_collection_json(void);
char* generate_manager_json(const char *manager_id);
char* generate_thermalequipment_collection_json(void);
char* generate_thermalequipment_json(const char *thermalequipment_id);
char* open_resource_json(json_resource_type_t type);

char* open_json_file(const char *file_name, json_resource_type_t type);
bool save_json_file(const char *file_name, const char *json_info);

#endif // REDFISH_RESOURCES_H
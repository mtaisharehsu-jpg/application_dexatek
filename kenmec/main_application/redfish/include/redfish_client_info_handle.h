#ifndef REDFISH_CLIENT_INFO_HANDLE_H
#define REDFISH_CLIENT_INFO_HANDLE_H

#include "redfish_resources.h"
#include <stddef.h>

#define MAX_TOKEN_LENGTH 64
#define MAX_USERNAME_LENGTH 64
#define MAX_PASSWORD_LENGTH 128
#define MAX_ROLE_LENGTH 64

#define SESSION_EXPIRY_SECONDS 300

typedef enum {
    DB_STATUS_OPEN_ERROR            = -1,
    DB_STATUS_PREPARE_ERROR         = -2,
    DB_STATUS_PASSWORD_NULL         = -3,
    DB_STATUS_SELECT_ERROR          = -4,
    DB_STATUS_PASSWORD_MISMATCH     = -5,
    DB_STATUS_USERNAME_MISMATCH     = -6,
    DB_STATUS_UNKNOW                = -7,
} db_status_type_t;

// Account information structure containing all data read from database
typedef struct {
    int id;                                    // Primary key from database
    char username[MAX_USERNAME_LENGTH];        // Account username
    char password[MAX_PASSWORD_LENGTH];        // Account password
    char role[MAX_ROLE_LENGTH];                // Account role/permissions
    bool enabled;                              // Whether account is enabled
    bool locked;                               // Whether account is locked
    char created_at[32];                       // Account creation timestamp
    char last_accessed[32];                    // Last access timestamp
} account_info_t;

typedef struct {
    int id;                                    // Primary key from database
    char token[MAX_TOKEN_LENGTH];              // Session token
    char username[MAX_USERNAME_LENGTH];        // Session username
    char role[MAX_ROLE_LENGTH];                // Session role
    char expiry[32];                           // Session expiry timestamp
} session_info_t;

int db_init(void);
int account_add(const char *username, const char *password, const char *role, http_response_t *response);
int account_update(int account_id, const char *new_password, const char *new_role, int *out_updated_password, int *out_updated_role);
int account_delete(int account_id);
int account_get(void);
int account_list_get(account_info_t* account_info_list);
int account_count_get(void);
int account_set_enabled(int account_id, bool enabled);
int account_set_locked(int account_id, bool locked);
int account_get_by_id(int account_id, account_info_t* account);
db_status_type_t account_check(const char *username, const char *password);

int session_add(const char *username, char* token_out, int* session_id_out);
int session_delete(const char *token);
int session_delete_by_id(int session_id);
int check_client_token(const http_request_t *request);
int cleanup_expired_sessions(void);
int dump_all_sessions(void);
int session_list_get(session_info_t* session_info_list);
int session_count_get(void);

// Retrieve authenticated identity (username and role) from request headers.
// Returns SUCCESS on success, FAIL otherwise.
int get_authenticated_identity(const http_request_t *request,
                               char *out_username, size_t out_username_size,
                               char *out_role, size_t out_role_size);

int redfish_get_uuid(char *uuid_out, size_t uuid_size);
int redfish_set_uuid(const char *uuid);

// SecurityPolicy model
typedef struct {
    int verify_certificate; // 0 or 1
} security_policy_t;

typedef enum {
    SYSTEM_CERTIFICATE_TYPE_SERVER_PRIVATE_KEY,
    SYSTEM_CERTIFICATE_TYPE_SERVER_CERTIFICATE,
    SYSTEM_CERTIFICATE_TYPE_ROOT_CERTIFICATE,

    SYSTEM_CERTIFICATE_TYPE_UNKNOWN,
} system_certificate_type_t;

// SecurityPolicy DB helpers (struct-based)
int security_policy_upsert(const char *manager_id, const security_policy_t *policy);
int security_policy_get(const char *manager_id, security_policy_t *out_policy);
int security_policy_delete(const char *manager_id);

// System Private Key (single row)
// Store/replace the single system private key PEM. Returns 0 on success.
int system_private_key_store_pem(const char *pem);
// Load the single system private key PEM. Returns number of bytes written, or negative on error.
int system_private_key_load_pem(char *pem_out, size_t pem_out_size);

// System Certificate (single row)
// Store/replace the single system certificate PEM. Returns 0 on success.
int system_certificate_store_pem(const char *pem);
// Load the single system certificate PEM. Returns number of bytes written, or negative on error.
int system_certificate_load_pem(char *pem_out, size_t pem_out_size);

// System Root Certificate (single row)
// Store/replace the single system root certificate PEM. Returns 0 on success.
int system_root_certificate_store_pem(const char *pem);
// Load the single system root certificate PEM. Returns number of bytes written, or negative on error.
int system_root_certificate_load_pem(char *pem_out, size_t pem_out_size);



#endif
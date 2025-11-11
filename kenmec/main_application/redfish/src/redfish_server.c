#include "dexatek/main_application/include/application_common.h"
#include "dexatek/main_application/include/utilities/os_utilities.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "config.h"
#include "redfish_resources.h"
#include "redfish_client_info_handle.h"
#include "redfish_server.h"
#include "redfish_init.h"

static const char *tag = "redfish_server";

int redfish_server_init(void) {
    debug(tag, "Redfish server initialized");
    return SUCCESS;
}

void redfish_server_cleanup(void) {
    debug(tag, "Redfish server cleanup completed");
}


int parse_http_request(const char *raw_request, http_request_t *request, int is_https) {
    if (!raw_request || !request) {
        return ERROR_INVALID_PARAM;
    }

    memset(request, 0, sizeof(http_request_t));
    request->is_https = is_https;

    char buffer[BUFFER_SIZE];
    strncpy(buffer, raw_request, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *header_end_marker = strstr(buffer, "\r\n\r\n");
    if (!header_end_marker) {
        return ERROR_INVALID_PARAM;
    }

    char *body_start = header_end_marker + 4;

    char *line_start = buffer;
    char *line_end = strstr(line_start, "\r\n");

    if (!line_end) {
        return ERROR_INVALID_PARAM;
    }
    *line_end = '\0';

    char *method = strtok(line_start, " ");
    char *path = strtok(NULL, " ");
    char *version = strtok(NULL, " ");

    if (!method || !path || !version) {
        return ERROR_INVALID_PARAM;
    }

    strncpy(request->method, method, sizeof(request->method) - 1);
    request->method[sizeof(request->method) - 1] = '\0';

    strncpy(request->path, path, sizeof(request->path) - 1);
    request->path[sizeof(request->path) - 1] = '\0';

    request->header_count = 0;
    line_start = line_end + 2;

    while (line_start < header_end_marker && (line_end = strstr(line_start, "\r\n")) != NULL) {
        *line_end = '\0';

        if (strlen(line_start) == 0) {
            break;
        }

        char *colon = strchr(line_start, ':');
        if (!colon) {
            line_start = line_end + 2;
            continue;
        }

        if (request->header_count >= MAX_HEADERS) {
            break;
        }

        *colon = '\0';
        char *key = line_start;
        char *value = colon + 1;

        while (*value == ' ') {
            value++;
        }

        strncpy(request->headers[request->header_count][0], key, sizeof(request->headers[0][0]) - 1);
        request->headers[request->header_count][0][sizeof(request->headers[0][0]) - 1] = '\0';

        strncpy(request->headers[request->header_count][1], value, sizeof(request->headers[0][1]) - 1);
        request->headers[request->header_count][1][sizeof(request->headers[0][1]) - 1] = '\0';

        request->header_count++;

        line_start = line_end + 2;
    }

    if (body_start && strlen(body_start) > 0) {
        strncpy(request->body, body_start, sizeof(request->body) - 1);
        request->body[sizeof(request->body) - 1] = '\0';
        request->content_length = strlen(request->body);
    } else {
        request->content_length = 0;
    }

    return SUCCESS;
}



static int generate_https_redirect_response(const http_request_t *request, http_response_t *response) {
    response->status_code = HTTP_TEMPORARY_REDIRECT;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    
    // Get Host header
    const char *host = NULL;
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i][0], "Host") == 0) {
            host = request->headers[i][1];
            break;
        }
    }
    
    // Build HTTPS URL
    char https_url[256];
    if (host && strstr(host, ":")) {
        // Extract hostname and replace port with HTTPS port
        char hostname[128];
        const char *colon = strchr(host, ':');
        if (colon) {
            size_t hostname_len = (size_t)(colon - host);
            if (hostname_len >= sizeof(hostname)) hostname_len = sizeof(hostname) - 1;
            strncpy(hostname, host, hostname_len);
            hostname[hostname_len] = '\0';
            snprintf(https_url, sizeof(https_url), "https://%s:%d%s", hostname, DEFAULT_PORT, request->path);
        } else {
            snprintf(https_url, sizeof(https_url), "https://%s:%d%s", host, DEFAULT_PORT, request->path);
        }
    } else if (host) {
        snprintf(https_url, sizeof(https_url), "https://%s:%d%s", host, DEFAULT_PORT, request->path);
    } else {
        snprintf(https_url, sizeof(https_url), "https://localhost:%d%s", DEFAULT_PORT, request->path);
    }
    
    // Set Location header
    if (response->header_count < MAX_HEADERS) {
        strcpy(response->headers[response->header_count][0], "Location");
        strncpy(response->headers[response->header_count][1], https_url, MAX_HEADER_VALUE_LEN - 1);
        response->headers[response->header_count][1][MAX_HEADER_VALUE_LEN - 1] = '\0';
        response->header_count++;
    }
    
    snprintf(response->body, sizeof(response->body),
             "{\"error\":\"Basic Authentication requires HTTPS\",\"Location\":\"%s\"}", https_url);
    response->content_length = strlen(response->body);
    return SUCCESS;
}

// Returns SUCCESS if the caller has ConfigureComponents privilege (Administrator or Operator).
// On failure, populates response with 403 InsufficientPrivilege and returns FAIL.
static int check_configure_components_privilege(const http_request_t *request, http_response_t *response)
{
    char req_username[128] = {0};
    char req_role[64] = {0};

    if (get_authenticated_identity(request, req_username, sizeof(req_username), req_role, sizeof(req_role)) == SUCCESS) {
        if (strcasecmp(req_role, "Administrator") == 0 || strcasecmp(req_role, "Operator") == 0) {
            return SUCCESS;
        }
    }

    response->status_code = HTTP_FORBIDDEN;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    strcpy(response->body,
        "{\n"
        "  \"error\": {\n"
        "    \"code\": \"Base.1.15.0.InsufficientPrivilege\",\n"
        "    \"message\": \"The resource or operation requires a higher privilege than the provided credentials.\"\n"
        "  }\n"
        "}\n");
    response->content_length = strlen(response->body);
    return FAIL;
}

// Enforce: Only Administrator may delete any session; non-admins may delete only their own
static int check_can_delete_session(const http_request_t *request, const char *id_or_token, int is_numeric_id, http_response_t *response)
{
    if (!request || !id_or_token || !response) {
        return FAIL;
    }

    char req_username[128] = {0};
    char req_role[64] = {0};
    if (get_authenticated_identity(request, req_username, sizeof(req_username), req_role, sizeof(req_role)) != SUCCESS) {
        response->status_code = HTTP_UNAUTHORIZED;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
                 "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"Authentication required\"}}"
        );
        response->content_length = strlen(response->body);
        return FAIL;
    }

    // Administrator can delete any session
    if (strcasecmp(req_role, "Administrator") == 0) {
        return SUCCESS;
    }

    // Lookup the session to get its owner username
    int total = session_count_get();
    if (total <= 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
                 "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"Session not found\"}}"
        );
        response->content_length = strlen(response->body);
        return FAIL;
    }

    session_info_t *sessions = (session_info_t*)calloc((size_t)total, sizeof(session_info_t));
    if (!sessions) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
                 "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"Out of memory\"}}"
        );
        response->content_length = strlen(response->body);
        return FAIL;
    }

    int count = session_list_get(sessions);
    const char *owner = NULL;
    if (count > 0) {
        if (is_numeric_id) {
            long want_id = strtol(id_or_token, NULL, 10);
            for (int i = 0; i < count; i++) {
                if (sessions[i].id == (int)want_id) {
                    owner = sessions[i].username;
                    break;
                }
            }
        } else {
            for (int i = 0; i < count; i++) {
                if (sessions[i].token[0] != '\0' && strcmp(sessions[i].token, id_or_token) == 0) {
                    owner = sessions[i].username;
                    break;
                }
            }
        }
    }

    if (!owner || owner[0] == '\0') {
        free(sessions);
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
                 "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"Session not found\"}}"
        );
        response->content_length = strlen(response->body);
        return FAIL;
    }

    // Non-admin: allow only if deleting own session
    int allowed = (strcasecmp(req_username, owner) == 0) ? 1 : 0;
    free(sessions);

    if (!allowed) {
        response->status_code = HTTP_FORBIDDEN;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body,
            "{\n"
            "  \"error\": {\n"
            "    \"code\": \"Base.1.15.0.InsufficientPrivilege\",\n"
            "    \"message\": \"The resource or operation requires a higher privilege than the provided credentials.\"\n"
            "  }\n"
            "}\n");
        response->content_length = strlen(response->body);
        return FAIL;
    }

    return SUCCESS;
}

// Returns SUCCESS if the caller has ConfigureUsers privilege (Administrator only).
// On failure, populates response with 403 InsufficientPrivilege and returns FAIL.
static int check_configure_users_privilege(const http_request_t *request, http_response_t *response)
{
    char req_username[128] = {0};
    char req_role[64] = {0};

    if (get_authenticated_identity(request, req_username, sizeof(req_username), req_role, sizeof(req_role)) == SUCCESS) {
        if (strcasecmp(req_role, "Administrator") == 0) {
            return SUCCESS;
        }
    }

    response->status_code = HTTP_FORBIDDEN;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    strcpy(response->body,
        "{\n"
        "  \"error\": {\n"
        "    \"code\": \"Base.1.15.0.InsufficientPrivilege\",\n"
        "    \"message\": \"The resource or operation requires a higher privilege than the provided credentials.\"\n"
        "  }\n"
        "}\n");
    response->content_length = strlen(response->body);
    return FAIL;
}

int process_redfish_request(const http_request_t *request, http_response_t *response) {
    if (!request || !response) {
        return ERROR_INVALID_PARAM;
    }

    memset(response, 0, sizeof(http_response_t));

    // Set default content type
    strcpy(response->content_type, CONTENT_TYPE_JSON);

    // Parse Redfish path
    char *resource_id = NULL;
    redfish_resource_type_t resource_type = parse_redfish_path(request->path, &resource_id);
    
    printf("DEBUG: resource_type = %d, resource_id = %s\n", resource_type, resource_id ? resource_id : "NULL");

    // Enforce OData-Version: 4.0 for requests that include OData-Version
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i][0], "OData-Version") == 0) {
            const char *ver = request->headers[i][1];
            if (strcmp(ver, "4.0") != 0) {
                response->status_code = HTTP_PRECONDITION_FAILED;
                strcpy(response->content_type, CONTENT_TYPE_JSON);
                snprintf(response->body, sizeof(response->body),
                         "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"Unsupported OData-Version\"}}"
                );
                response->content_length = strlen(response->body);
                add_cors_headers(response);
                return SUCCESS;
            }
            break;
        }
    }

#if defined(CONFIG_REDFISH_TOKEN_VERIFY_ENABLE) && CONFIG_REDFISH_TOKEN_VERIFY_ENABLE
    // Public, unauthenticated endpoints (per Redfish spec):
    // - /redfish
    // - /redfish/v1
    // - /redfish/v1/odata
    // - /redfish/v1/$metadata
    bool is_public_resource = (
        resource_type == REDFISH_RESOURCE_VERSION ||
        resource_type == REDFISH_RESOURCE_SERVICE_ROOT ||
        resource_type == REDFISH_RESOURCE_ODATA_SERVICE ||
        resource_type == REDFISH_RESOURCE_ODATA_METADATA
    );

    if (!is_public_resource) {
        if (check_client_token(request) != SUCCESS) {
            error(tag, "token check failed");
            return handle_token_invalid(response);
        }
    }
#endif

    // Handle different HTTP methods
    if (strcmp(request->method, HTTP_METHOD_GET) == 0 || strcmp(request->method, HTTP_METHOD_HEAD) == 0) {
        // Enforce auth for Sessions collection on GET
        if (resource_type == REDFISH_RESOURCE_SESSIONSERVICE_SESSIONS) {
            bool has_auth = false;
            for (int i = 0; i < request->header_count; i++) {
                if (strcasecmp(request->headers[i][0], "X-Auth-Token") == 0) {
                    has_auth = true; break;
                }
                if (strcasecmp(request->headers[i][0], "Authorization") == 0) {
                    has_auth = true; break;
                }
            }
            if (!has_auth) {
                response->status_code = HTTP_UNAUTHORIZED;
                strcpy(response->content_type, CONTENT_TYPE_JSON);
                snprintf(response->body, sizeof(response->body),
                         "{\"error\":\"Authentication required\"}");
                response->content_length = strlen(response->body);
                
                // Add WWW-Authenticate header for 401 responses
                if (response->header_count < MAX_HEADERS) {
                    strcpy(response->headers[response->header_count][0], "WWW-Authenticate");
                    strcpy(response->headers[response->header_count][1], "Basic realm=\"Redfish\", Bearer");
                    response->header_count++;
                }
                
                add_cors_headers(response);
                return SUCCESS;
            }
        }
        
        int handler_result = SUCCESS;
        switch (resource_type) {
            case REDFISH_RESOURCE_VERSION:
                handler_result = handle_version_root(response);
                break;
            case REDFISH_RESOURCE_SERVICE_ROOT:
                handler_result = handle_service_root(response);
                break;
            case REDFISH_RESOURCE_CERTIFICATESERVICE:
                handler_result = handle_certificate_service(response);
                break;
            case REDFISH_RESOURCE_ODATA_SERVICE:
                handler_result = handle_odata_service(response);
                break;
            case REDFISH_RESOURCE_ODATA_METADATA:
                handler_result = handle_odata_metadata(response);
                break;
            case REDFISH_RESOURCE_UPDATESERVICE:
                handler_result = handle_update_service(response);
                break;
            case REDFISH_RESOURCE_ACCOUNTSERVICE:
                handler_result = handle_account_service(response);
                break;

            case REDFISH_RESOURCE_ACCOUNTSERVICE_ACCOUNTS:
                handler_result = handle_accounts_collection(response);
                break;

            case REDFISH_RESOURCE_ACCOUNTSERVICE_ACCOUNT:
                handler_result = handle_account_member(resource_id, response);
                break;

            case REDFISH_RESOURCE_CHASSIS_COLLECTION:
                handler_result = handle_chassis_collection(response);
                break;

            case REDFISH_RESOURCE_CHASSIS:
                handler_result = handle_chassis(resource_id, response);
                break;

            case REDFISH_RESOURCE_SYSTEMS_COLLECTION:
                handler_result = handle_systems_collection(response);
                break;

            case REDFISH_RESOURCE_SYSTEM:
                handler_result = handle_system(resource_id, response);
                break;

            case REDFISH_RESOURCE_MANAGERS_ETHERNET_INTERFACE_ETH0:
                handler_result = get_managers_ethernet_interface_eth0(resource_id, response);
                break;
            
            case REDFISH_RESOURCE_MANAGERS_ETHERNET_INTERFACE:
                handler_result = get_managers_ethernet_interface(resource_id, response);
                break;

            case REDFISH_RESOURCE_MANAGERS_KENMEC:
                handler_result = get_managers_kenmec_resource(resource_id, response);
                break;

            case REDFISH_RESOURCE_MANAGER_SECURITY_POLICY:
                handler_result = get_manager_security_policy(resource_id, response);
                break;
            case REDFISH_RESOURCE_MANAGER_SECURITY_POLICY_TRUSTED_CERTIFICATES:
                handler_result = handle_manager_security_policy_trusted_certificates(response);
                break;
            case REDFISH_RESOURCE_MANAGER_SECURITY_POLICY_TRUSTED_CERTIFICATE:
                handler_result = handle_manager_security_policy_trusted_certificate(resource_id, response);
                break;
            case REDFISH_RESOURCE_MANAGER_NETWORK_PROTOCOL:
                handler_result = handle_manager_network_protocol(resource_id, response);
                break;
            case REDFISH_RESOURCE_MANAGER_NETWORK_PROTOCOL_HTTPS_CERTIFICATES:
                handler_result = handle_manager_network_protocol_https_certificates(resource_id, response);
                break;
            case REDFISH_RESOURCE_MANAGER_NETWORK_PROTOCOL_HTTPS_CERTIFICATE: {
                // Extract manager_id from the request path and use resource_id as cert_id
                const char *prefix = "/redfish/v1/Managers/";
                static char manager_id_buf[64];
                manager_id_buf[0] = '\0';
                const char *base = request->path;
                if (strncmp(base, prefix, strlen(prefix)) == 0) {
                    const char *p = base + strlen(prefix); // points at {id}/...
                    const char *slash = strchr(p, '/');
                    if (slash) {
                        size_t n = (size_t)(slash - p);
                        if (n >= sizeof(manager_id_buf)) n = sizeof(manager_id_buf) - 1;
                        strncpy(manager_id_buf, p, n);
                        manager_id_buf[n] = '\0';
                    }
                }
                const char *cert_id = resource_id;
                const char *mgr = (manager_id_buf[0] != '\0') ? manager_id_buf : MANAGER_ID_KENMEC;
                handler_result = handle_manager_network_protocol_https_certificate_member(mgr, cert_id, response);
                break;
            }
            case REDFISH_RESOURCE_MANAGERS_COLLECTION:
                handler_result = get_managers_collection(resource_id, response);
                break;

            case REDFISH_RESOURCE_MANAGER:
                handler_result = handle_manager(resource_id, response);
                break;

            case REDFISH_RESOURCE_THERMALEQUIPMENT_COLLECTION:
                handler_result = handle_thermalequipment_collection(response);
                break;

            case REDFISH_RESOURCE_THERMALEQUIPMENT:
                handler_result = handle_thermalequipment(resource_id, response);
                break;

            case REDFISH_RESOURCE_CDU_OEM:
                handler_result = handle_cdu_oem(resource_id, response);
                break;

            case REDFISH_RESOURCE_CDU_OEM_KENMEC:
                handler_result = handle_cdu_oem_kenmec(resource_id, response);
                break;

            case REDFISH_RESOURCE_CDU_OEM_IOBOARDS:
                handler_result = handle_cdu_oem_ioboards(resource_id, response); 
                break;

            case REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS: {
                handler_result = handle_cdu_oem_control_logics(resource_id, response);
                break;
            }

            case REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS_ACTION_READ: {
                const char *anchor = "/Oem/Kenmec/ControlLogics/";
                const char *pos = strstr(request->path, anchor);
                const char *member = NULL;
                if (pos) {
                    pos += strlen(anchor);
                    const char *slash = strchr(pos, '/');
                    if (slash) {
                        static char member_id[16];
                        size_t n = (size_t)(slash - pos);
                        if (n >= sizeof(member_id)) n = sizeof(member_id) - 1;
                        strncpy(member_id, pos, n);
                        member_id[n] = '\0';
                        member = member_id;
                    }
                }
                return handle_cdu_oem_control_logics_action_read(resource_id, member, request, response);
            }            

            case REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS_MEMBER: {
                // Need to extract member ID from the path tail after anchor
                const char *anchor = "/Oem/Kenmec/ControlLogics/";
                const char *pos = strstr(request->path, anchor);
                const char *member = pos ? pos + strlen(anchor) : NULL;
                if (!member || *member == '\0') {
                    response->status_code = HTTP_NOT_FOUND;
                    strcpy(response->content_type, "application/json");
                    snprintf(response->body, sizeof(response->body),
                        "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI %s was not found.\"}}",
                        request->path);
                    response->content_length = strlen(response->body);
                    return SUCCESS;
                }
                return handle_cdu_oem_control_logics_member(resource_id, member, response);
            }            

            case REDFISH_RESOURCE_CDU_OEM_IOBOARD_MEMBER: {
                // Need to extract member ID from the path tail after anchor
                const char *anchor = "/Oem/Kenmec/IOBoards/";
                const char *pos = strstr(request->path, anchor);
                const char *member = pos ? pos + strlen(anchor) : NULL;
                if (!member || *member == '\0') {
                    response->status_code = HTTP_NOT_FOUND;
                    strcpy(response->content_type, "application/json");
                    snprintf(response->body, sizeof(response->body),
                        "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI %s was not found.\"}}",
                        request->path);
                    response->content_length = strlen(response->body);
                    handler_result = SUCCESS;
                    break;
                }
                handler_result = handle_cdu_oem_ioboard_member(resource_id, member, response);
                break;
            }

            case REDFISH_RESOURCE_CDU_OEM_IOBOARD_ACTION_READ: {
                // Extract member id from path for the Read action on GET
                const char *anchor = "/Oem/Kenmec/IOBoards/";
                const char *pos = strstr(request->path, anchor);
                const char *member = NULL;
                if (pos) {
                    pos += strlen(anchor);
                    const char *slash = strchr(pos, '/');
                    if (slash) {
                        static char member_id[16];
                        size_t n = (size_t)(slash - pos);
                        if (n >= sizeof(member_id)) n = sizeof(member_id) - 1;
                        strncpy(member_id, pos, n);
                        member_id[n] = '\0';
                        member = member_id;
                    }
                }
                handler_result = handle_cdu_oem_ioboard_action_read(resource_id, member, request, response);
                break;
            }

            case REDFISH_RESOURCE_CDU_OEM_KENMEC_CONFIG_READ:
                return handle_cdu_oem_kenmec_config_read(resource_id, response);

            case REDFISH_RESOURCE_SESSIONSERVICE:
                handler_result = handle_session_service(response);
                break;
            case REDFISH_RESOURCE_SESSIONSERVICE_SESSIONS:
                {
                    // If path is a specific session member, serve the member
                    const char *prefix = "/redfish/v1/SessionService/Sessions/";
                    if (strncmp(request->path, prefix, strlen(prefix)) == 0 && strlen(request->path) > strlen(prefix)) {
                        const char *sid = request->path + strlen(prefix);
                        handler_result = handle_session_member(sid, response);
                    } else {
                        handler_result = handle_sessions_collection(request, response);
                    }
                }
                break;

            case REDFISH_RESOURCE_ACCOUNTSERVICE_ROLES_COLLECTION:
                handler_result = handle_roles_collection(response);
                break;

            case REDFISH_RESOURCE_ACCOUNTSERVICE_ROLE:
                handler_result = handle_role_member(resource_id, response);
                break;

            default:
                response->status_code = HTTP_NOT_FOUND;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, sizeof(response->body),
                    "{"
                      "\"error\":{"
                        "\"code\":\"Base.1.15.0.ResourceMissingAtURI\","
                        "\"message\":\"The resource at the URI %s was not found.\","
                        "\"@Message.ExtendedInfo\":[{"
                          "\"@odata.type\":\"#Message.v1_1_1.Message\","
                          "\"MessageId\":\"Base.1.15.0.ResourceMissingAtURI\","
                          "\"Message\":\"The resource at the URI %s was not found.\","
                          "\"MessageArgs\":[\"%s\"],"
                          "\"Severity\":\"Critical\","
                          "\"Resolution\":\"Provide a valid URI and resubmit the request.\""
                        "}]"
                      "}"
                    "}",
                    request->path, request->path, request->path);
                response->content_length = strlen(response->body);
                handler_result = SUCCESS;
                break;
        }
        
        // Add Allow header for GET/HEAD requests (resource-specific)
        if (response->header_count < MAX_HEADERS) {
            char allowed_methods[64];
            // Default: only GET and HEAD
            strcpy(allowed_methods, "GET, HEAD");
            // Sessions: collection supports POST, but member does not
            if (resource_type == REDFISH_RESOURCE_SESSIONSERVICE_SESSIONS) {
                const char *sess_prefix = "/redfish/v1/SessionService/Sessions/";
                if (strncmp(request->path, sess_prefix, strlen(sess_prefix)) == 0 && request->path[strlen(sess_prefix)] != '\0') {
                    // Member URI: keep default GET, HEAD
                } else {
                    // Collection URI: allow POST as well
                    strcpy(allowed_methods, "GET, HEAD, POST");
                }
            }
            // Accounts collection supports POST (create account)
            if (resource_type == REDFISH_RESOURCE_ACCOUNTSERVICE_ACCOUNTS) {
                strcpy(allowed_methods, "GET, HEAD, POST");
            }
            // Account members support PATCH (update) and DELETE
            if (resource_type == REDFISH_RESOURCE_ACCOUNTSERVICE_ACCOUNT) {
                strcpy(allowed_methods, "GET, HEAD, PATCH, DELETE");
            }
            // ServiceRoot must not advertise POST/PUT/PATCH/DELETE
            if (resource_type == REDFISH_RESOURCE_SERVICE_ROOT || resource_type == REDFISH_RESOURCE_VERSION) {
                strcpy(allowed_methods, "GET, HEAD");
            }

            strcpy(response->headers[response->header_count][0], "Allow");
            strcpy(response->headers[response->header_count][1], allowed_methods);
            response->header_count++;
        }
        
        // Add Link header for GET/HEAD requests (Redfish spec requirement)
        if (response->header_count < MAX_HEADERS) {
            strcpy(response->headers[response->header_count][0], "Link");
            strcpy(response->headers[response->header_count][1], "</redfish/v1/$metadata>; rel=\"describedby\"");
            response->header_count++;
        }
        
        // Check if handler requested HTTPS redirect
        if (handler_result == ERROR_REQUIRES_HTTPS) {
            return generate_https_redirect_response(request, response);
        }
        
        // For HEAD requests, clear the body but keep headers
        if (strcmp(request->method, HTTP_METHOD_HEAD) == 0) {
            response->body[0] = '\0';
            response->content_length = 0;
        }
        
        return handler_result;
        
    } else if (strcmp(request->method, HTTP_METHOD_POST) == 0) {
        // Handle POST requests (create new resources)
        int handler_result = SUCCESS;
        switch (resource_type) {
            case REDFISH_RESOURCE_MANAGER_SECURITY_POLICY:
                handler_result = post_manager_security_policy(resource_id, request, response);
                break;
            case REDFISH_RESOURCE_SESSIONSERVICE_SESSIONS_MEMBERS:
                // Members POST is equivalent to collection POST
                handler_result = handle_sessions(request, response);
                break;
            case REDFISH_RESOURCE_CHASSIS_COLLECTION:
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                handler_result = handle_chassis_create(request, response);
                break;
            case REDFISH_RESOURCE_SYSTEMS_COLLECTION:
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                handler_result = handle_system_create(request, response);
                break;
            case REDFISH_RESOURCE_MANAGERS_COLLECTION:
                handler_result = handle_manager_create(request, response);
                break;
            case REDFISH_RESOURCE_THERMALEQUIPMENT_COLLECTION:
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                handler_result = handle_thermalequipment_create(request, response);
                break;
            case REDFISH_RESOURCE_SESSIONSERVICE_SESSIONS:
                handler_result = handle_sessions(request, response);
                break;

            case REDFISH_RESOURCE_ACCOUNTSERVICE:
                handler_result = handle_accounts();
                break;
            case REDFISH_RESOURCE_ACCOUNTSERVICE_ACCOUNTS:
                if (check_configure_users_privilege(request, response) != SUCCESS) return SUCCESS;
                handler_result = handle_accounts_create(request, response);
                break;            
            case REDFISH_RESOURCE_CDU_OEM_KENMEC_CONFIG_WRITE:
                return handle_cdu_oem_kenmec_config_write(resource_id, request, response);

            case REDFISH_RESOURCE_CDU_OEM_IOBOARD_ACTION_WRITE: {
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                const char *anchor = "/Oem/Kenmec/IOBoards/";
                const char *pos = strstr(request->path, anchor);
                const char *member = NULL;
                if (pos) {
                    pos += strlen(anchor);
                    const char *slash = strchr(pos, '/');
                    if (slash) {
                        static char member_id[16];
                        size_t n = (size_t)(slash - pos);
                        if (n >= sizeof(member_id)) n = sizeof(member_id) - 1;
                        strncpy(member_id, pos, n);
                        member_id[n] = '\0';
                        member = member_id;
                    }
                }
                handler_result = handle_cdu_oem_ioboard_action_write(resource_id, member, request, response);
                break;
            }

            case REDFISH_RESOURCE_MANAGER_RESET_ACTION: {
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                handler_result = handle_manager_reset_action(resource_id, request, response);
                break;
            }

            case REDFISH_RESOURCE_CERTIFICATESERVICE_GENERATE_CSR: {
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                handler_result = handle_certificate_service_generate_csr(request, response);
                break;
            }
            case REDFISH_RESOURCE_CERTIFICATESERVICE_REPLACE_CERTIFICATE: {
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                handler_result = handle_certificate_service_replace_certificate(request, response);
                break;
            }

            case REDFISH_RESOURCE_UPDATESERVICE_MULTIPART:
                handler_result = handle_update_service_multipart_upload(request, response);
                break;

            case REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS_ACTION_WRITE: {
                const char *anchor = "/Oem/Kenmec/ControlLogics/";
                const char *pos = strstr(request->path, anchor);
                const char *member = NULL;
                if (pos) {
                    pos += strlen(anchor);
                    const char *slash = strchr(pos, '/');
                    if (slash) {
                        static char member_id[16];
                        size_t n = (size_t)(slash - pos);
                        if (n >= sizeof(member_id)) n = sizeof(member_id) - 1;
                        strncpy(member_id, pos, n);
                        member_id[n] = '\0';
                        member = member_id;
                    }
                }
                return handle_cdu_oem_control_logics_action_write(resource_id, member, request, response);
            }

            default:
                response->status_code = HTTP_METHOD_NOT_ALLOWED;
                strcpy(response->body, "{\"error\":\"POST not allowed on this resource\"}");
                response->content_length = strlen(response->body);
                // Add Allow header for 405 response
                if (response->header_count < MAX_HEADERS) {
                    strcpy(response->headers[response->header_count][0], "Allow");
                    strcpy(response->headers[response->header_count][1], "GET, HEAD");
                    response->header_count++;
                }
                add_cors_headers(response);
                handler_result = SUCCESS;
                break;
        }
        
        // Check if handler requested HTTPS redirect
        if (handler_result == ERROR_REQUIRES_HTTPS) {
            return generate_https_redirect_response(request, response);
        }
        
        return handler_result;
    } else if (strcmp(request->method, HTTP_METHOD_PUT) == 0) {
        // Handle PUT requests (update existing resources)
        switch (resource_type) {
            case REDFISH_RESOURCE_CHASSIS:
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                return handle_chassis_update(resource_id, request, response);

            case REDFISH_RESOURCE_SYSTEM:
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                return handle_system_update(resource_id, request, response);

            case REDFISH_RESOURCE_MANAGER:
                return handle_manager_update(resource_id, request, response);

            case REDFISH_RESOURCE_THERMALEQUIPMENT:
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                return handle_thermalequipment_update(resource_id, request, response);

            default:
                response->status_code = HTTP_METHOD_NOT_ALLOWED;
                strcpy(response->body, "{\"error\":\"PUT not allowed on this resource\"}");
                response->content_length = strlen(response->body);
                // Add Allow header for 405 response
                if (response->header_count < MAX_HEADERS) {
                    strcpy(response->headers[response->header_count][0], "Allow");
                    strcpy(response->headers[response->header_count][1], "GET, HEAD");
                    response->header_count++;
                }
                add_cors_headers(response);
                return SUCCESS;
        }
    } else if (strcmp(request->method, HTTP_METHOD_DELETE) == 0) {
        // Handle DELETE requests (remove resources)
        switch (resource_type) {
            case REDFISH_RESOURCE_MANAGER_SECURITY_POLICY:
                return delete_manager_security_policy(resource_id, response);
            case REDFISH_RESOURCE_CHASSIS:
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                return handle_chassis_delete(resource_id, response);

            case REDFISH_RESOURCE_SYSTEM:
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                return handle_system_delete(resource_id, response);

            case REDFISH_RESOURCE_MANAGER:
                return handle_manager_delete(resource_id, response);

            case REDFISH_RESOURCE_THERMALEQUIPMENT:
                if (check_configure_components_privilege(request, response) != SUCCESS) return SUCCESS;
                return handle_thermalequipment_delete(resource_id, response);

            case REDFISH_RESOURCE_ACCOUNTSERVICE_ACCOUNT:
                if (check_configure_users_privilege(request, response) != SUCCESS) return SUCCESS;
                return handle_account_delete(resource_id, response);

            case REDFISH_RESOURCE_SESSIONSERVICE_SESSIONS: {
                // Expect path /redfish/v1/SessionService/Sessions/{id_or_token}
                const char *prefix = "/redfish/v1/SessionService/Sessions/";
                const char *p = strstr(request->path, prefix);
                const char *id_or_token = NULL;
                if (p == request->path) {
                    id_or_token = request->path + strlen(prefix);
                } else if (p) {
                    id_or_token = p + strlen(prefix);
                }
                if (id_or_token && *id_or_token) {
                    // Check if it's a numeric ID (1, 2, 3, etc.) or a token
                    char *endptr;
                    long session_id = strtol(id_or_token, &endptr, 10);
                    if (*endptr == '\0' && session_id > 0) {
                        // It's a numeric ID
                        if (check_can_delete_session(request, id_or_token, 1, response) != SUCCESS) {
                            return SUCCESS; // check_can_delete_session already set the response
                        }
                        int rc = session_delete_by_id((int)session_id);
                        if (rc == SUCCESS) {
                            response->status_code = HTTP_NO_CONTENT;
                            response->body[0] = '\0';
                            response->content_length = 0;
                            return SUCCESS;
                        } else {
                            response->status_code = HTTP_NOT_FOUND;
                            strcpy(response->content_type, CONTENT_TYPE_JSON);
                            snprintf(response->body, sizeof(response->body),
                                     "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"Session not found\"}}"
                            );
                            response->content_length = strlen(response->body);
                            return SUCCESS;
                        }
                    } else {
                        // It's a token
                        if (check_can_delete_session(request, id_or_token, 0, response) != SUCCESS) {
                            return SUCCESS;
                        }
                        return handle_session_delete(id_or_token, response);
                    }
                }
                response->status_code = HTTP_BAD_REQUEST;
                strcpy(response->content_type, CONTENT_TYPE_JSON);
                snprintf(response->body, sizeof(response->body),
                         "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"Missing session ID or token in URI\"}}"
                );
                response->content_length = strlen(response->body);
                return SUCCESS;
            }

            default:
                response->status_code = HTTP_METHOD_NOT_ALLOWED;
                strcpy(response->body, "{\"error\":\"DELETE not allowed on this resource\"}");
                response->content_length = strlen(response->body);
                // Add Allow header for 405 response
                if (response->header_count < MAX_HEADERS) {
                    strcpy(response->headers[response->header_count][0], "Allow");
                    strcpy(response->headers[response->header_count][1], "GET, HEAD");
                    response->header_count++;
                }
                add_cors_headers(response);
                return SUCCESS;
        }
    } else if (strcmp(request->method, HTTP_METHOD_PATCH) == 0) {
        // Handle PATCH requests (update resources)
        switch (resource_type) {
            case REDFISH_RESOURCE_MANAGER_SECURITY_POLICY:
                return patch_manager_security_policy(resource_id, request, response);
            case REDFISH_RESOURCE_MANAGERS_ETHERNET_INTERFACE_ETH0 :
                return patch_managers_ethernet_interface_eth0(resource_id, request, response); 
            
            case REDFISH_RESOURCE_ACCOUNTSERVICE_ACCOUNT:
                return handle_account_patch(resource_id, request, response);
            
            default:
                response->status_code = HTTP_METHOD_NOT_ALLOWED;
                strcpy(response->body, "{\"error\":\"PATCH not allowed on this resource\"}");
                response->content_length = strlen(response->body);
                // Add Allow header for 405 response
                if (response->header_count < MAX_HEADERS) {
                    strcpy(response->headers[response->header_count][0], "Allow");
                    strcpy(response->headers[response->header_count][1], "GET, HEAD");
                    response->header_count++;
                }
                add_cors_headers(response);
                return SUCCESS;
        }
        
        
    } else if (strcmp(request->method, HTTP_METHOD_POST) == 0) {
        // Handle POST for multipart upload endpoint
        if (strcmp(request->path, "/redfish/v1/UpdateService/MultipartUpload") == 0 ||
            strcmp(request->path, "/UpdateFirmwareMultipart") == 0) {
            return handle_update_service_multipart_upload(request, response);
        }
        // Fallback: method not allowed
        response->status_code = HTTP_METHOD_NOT_ALLOWED;
        strcpy(response->body, "{\"error\":\"POST not allowed on this resource\"}");
        response->content_length = strlen(response->body);
        if (response->header_count < MAX_HEADERS) {
            strcpy(response->headers[response->header_count][0], "Allow");
            strcpy(response->headers[response->header_count][1], "GET, HEAD");
            response->header_count++;
        }
        add_cors_headers(response);
        return SUCCESS;
    } else {
        // Method not allowed
        response->status_code = HTTP_METHOD_NOT_ALLOWED;
        strcpy(response->body, "{\"error\":\"Method not allowed\"}");
        response->content_length = strlen(response->body);
        // Add Allow header for 405 response
        if (response->header_count < MAX_HEADERS) {
            strcpy(response->headers[response->header_count][0], "Allow");
            strcpy(response->headers[response->header_count][1], "GET, HEAD");
            response->header_count++;
        }
        add_cors_headers(response);
        return SUCCESS;
    }

    response->content_length = strlen(response->body);
    add_cors_headers(response);

    // Add Allow header for non-GET/HEAD responses (405 errors, etc.)
    if (response->status_code == HTTP_METHOD_NOT_ALLOWED && response->header_count < MAX_HEADERS) {
        strcpy(response->headers[response->header_count][0], "Allow");
        strcpy(response->headers[response->header_count][1], "GET, POST, PUT, PATCH, DELETE, HEAD");
        response->header_count++;
    }

    return SUCCESS;
}

void generate_http_response(const http_response_t *response, char *output, size_t output_size) {
    if (!response || !output) return;

    const char *status_text = get_http_status_text(response->status_code);

    char dynamic_headers[2048];
    dynamic_headers[0] = '\0';

    for (int i = 0; i < response->header_count; i++) {
        const char *name = response->headers[i][0];
        const char *value = response->headers[i][1];
        if (!name || !value || name[0] == '\0') continue;

        // Skip headers that are already emitted explicitly below
        if (strcasecmp(name, "Content-Type") == 0) continue;
        if (strcasecmp(name, "Content-Length") == 0) continue;
        if (strcasecmp(name, "Access-Control-Allow-Origin") == 0) continue;
        if (strcasecmp(name, "Access-Control-Allow-Headers") == 0) continue;
        if (strcasecmp(name, "Cache-Control") == 0) continue;

        char line[256];
        snprintf(line, sizeof(line), "%s: %s\r\n", name, value);
        strncat(dynamic_headers, line, sizeof(dynamic_headers) - strlen(dynamic_headers) - 1);
    }

    // Fallback to application/json if content_type not set
    const char *effective_content_type = response->content_type && response->content_type[0] ?
        response->content_type : "application/json";

    snprintf(output, output_size,
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Access-Control-Allow-Origin: *\r\n"
            //  "Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE\r\n"
             "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
             "Cache-Control: no-store\r\n"
             "OData-Version: 4.0\r\n"
             "%s"
             "\r\n"
             "%s",
             response->status_code, status_text,
             effective_content_type,
             response->content_length,
             dynamic_headers,
             response->body);
}

redfish_resource_type_t parse_redfish_path(const char *path, char **resource_id) {
    if (!path) return REDFISH_RESOURCE_UNKNOWN;

    printf("Path: %s **************\n", path);

    // Remove leading and trailing slashes
    while (*path == '/') path++;

    if (strcmp(path, "redfish") == 0 || strcmp(path, "redfish/") == 0) {
        return REDFISH_RESOURCE_VERSION;
    }

    if (strcmp(path, "redfish/v1") == 0 || strcmp(path, "redfish/v1/") == 0) {
        return REDFISH_RESOURCE_SERVICE_ROOT;
    }

    if (strcmp(path, "redfish/v1/odata") == 0) {
        return REDFISH_RESOURCE_ODATA_SERVICE;
    }

    if (strcmp(path, "redfish/v1/$metadata") == 0) {
        return REDFISH_RESOURCE_ODATA_METADATA;
    }

    if (strcmp(path, "redfish/v1/CertificateService") == 0) {
        return REDFISH_RESOURCE_CERTIFICATESERVICE;
    }
    if (strcmp(path, "redfish/v1/UpdateService") == 0) {
        return REDFISH_RESOURCE_UPDATESERVICE;
    }
    
    if (strcmp(path, "UpdateFirmwareMultipart") == 0) {
        return REDFISH_RESOURCE_UPDATESERVICE_MULTIPART;
    }

    if (strcmp(path, "redfish/v1/CertificateService/Actions/CertificateService.GenerateCSR") == 0) {
        return REDFISH_RESOURCE_CERTIFICATESERVICE_GENERATE_CSR;
    }
    if (strcmp(path, "redfish/v1/CertificateService/Actions/CertificateService.ReplaceCertificate") == 0) {
        return REDFISH_RESOURCE_CERTIFICATESERVICE_REPLACE_CERTIFICATE;
    }

    if (strncmp(path, "redfish/v1/Chassis", 18) == 0) {
        if (strlen(path) == 18) {
            return REDFISH_RESOURCE_CHASSIS_COLLECTION;
        }
        if (path[18] == '/') {
            *resource_id = (char*)(path + 19);
            return REDFISH_RESOURCE_CHASSIS;
        }
    }

    if (strncmp(path, "redfish/v1/Systems", 18) == 0) {
        if (strlen(path) == 18) {
            return REDFISH_RESOURCE_SYSTEMS_COLLECTION;
        }
        if (path[18] == '/') {
            *resource_id = (char*)(path + 19);
            return REDFISH_RESOURCE_SYSTEM;
        }
    }

    if (strncmp(path, "redfish/v1/Managers", 19) == 0) {
        if (strlen(path) == 19) {
            *resource_id = (char*)(path + 11);
            return REDFISH_RESOURCE_MANAGERS_COLLECTION;
        }
        if (strlen(path) == 26 && (strncmp(&path[20], MANAGER_ID_KENMEC, 6) == 0)) {
            *resource_id = (char*)(path + 20);
            return REDFISH_RESOURCE_MANAGERS_KENMEC;
        }

        // SecurityPolicy and related endpoints: /redfish/v1/Managers/{id}/SecurityPolicy*
        {
            const char *prefix = "redfish/v1/Managers/";
            if (strncmp(path, prefix, strlen(prefix)) == 0) {
                const char *p = path + strlen(prefix); // points at {id}...
                const char *slash = strchr(p, '/');
                if (slash) {
                    // Extract manager ID
                    static char manager_id[64];
                    size_t n = (size_t)(slash - p);
                    if (n >= sizeof(manager_id)) n = sizeof(manager_id) - 1;
                    strncpy(manager_id, p, n);
                    manager_id[n] = '\0';
                    *resource_id = manager_id;
                    
                    const char *remaining = slash + 1;
                    
                    // NetworkProtocol: /redfish/v1/Managers/{id}/NetworkProtocol
                    if (strcmp(remaining, "NetworkProtocol") == 0) {
                        return REDFISH_RESOURCE_MANAGER_NETWORK_PROTOCOL;
                    }
                    // NetworkProtocol HTTPS Certificates collection: /redfish/v1/Managers/{id}/NetworkProtocol/HTTPS/Certificates
                    if (strcmp(remaining, "NetworkProtocol/HTTPS/Certificates") == 0) {
                        return REDFISH_RESOURCE_MANAGER_NETWORK_PROTOCOL_HTTPS_CERTIFICATES;
                    }
                    printf("remaining: %s\n", remaining);
                    // NetworkProtocol HTTPS Certificate member: /redfish/v1/Managers/{id}/NetworkProtocol/HTTPS/Certificates/{id}
                    {
                        const char *cert_prefix = "NetworkProtocol/HTTPS/Certificates/";
                        size_t cert_prefix_len = strlen(cert_prefix);
                        if (strncmp(remaining, cert_prefix, cert_prefix_len) == 0 && remaining[cert_prefix_len] != '\0') {
                            static char certificate_id[64];
                            const char *cid = remaining + cert_prefix_len;
                            // Copy until end or next slash
                            size_t n = strcspn(cid, "/");
                            if (n >= sizeof(certificate_id)) n = sizeof(certificate_id) - 1;
                            strncpy(certificate_id, cid, n);
                            certificate_id[n] = '\0';
                            *resource_id = certificate_id;
                            return REDFISH_RESOURCE_MANAGER_NETWORK_PROTOCOL_HTTPS_CERTIFICATE;
                        }
                    }

                    // SecurityPolicy: /redfish/v1/Managers/{id}/SecurityPolicy
                    if (strcmp(remaining, "SecurityPolicy") == 0) {
                        return REDFISH_RESOURCE_MANAGER_SECURITY_POLICY;
                    }
                    
                    // TrustedCertificates Collection: /redfish/v1/Managers/{id}/SecurityPolicy/TLS/Server/TrustedCertificates
                    if (strcmp(remaining, "SecurityPolicy/TLS/Server/TrustedCertificates") == 0) {
                        return REDFISH_RESOURCE_MANAGER_SECURITY_POLICY_TRUSTED_CERTIFICATES;
                    }
                    
                    // TrustedCertificate Individual: /redfish/v1/Managers/{id}/SecurityPolicy/TLS/Server/TrustedCertificates/{id}
                    if (strncmp(remaining, "SecurityPolicy/TLS/Server/TrustedCertificates/", 46) == 0) {
                        *resource_id = (char*)(remaining + 46);
                        return REDFISH_RESOURCE_MANAGER_SECURITY_POLICY_TRUSTED_CERTIFICATE;
                    }
                }
            }
        }

        // Manager.Reset Action: /redfish/v1/Managers/{id}/Actions/Manager.Reset
        {
            const char *prefix = "redfish/v1/Managers/";
            if (strncmp(path, prefix, strlen(prefix)) == 0) {
                const char *p = path + strlen(prefix); // points at {id}...
                const char *slash = strchr(p, '/');
                if (slash && strncmp(slash + 1, "Actions/Manager.Reset", 21) == 0 && (slash[22] == '\0')) {
                    static char manager_id[64];
                    size_t n = (size_t)(slash - p);
                    if (n >= sizeof(manager_id)) n = sizeof(manager_id) - 1;
                    strncpy(manager_id, p, n);
                    manager_id[n] = '\0';
                    *resource_id = manager_id;
                    return REDFISH_RESOURCE_MANAGER_RESET_ACTION;
                }
            }
        }

        if (strlen(path) == 45 && (strncmp(&path[27], "EthernetInterfaces", 18) == 0)) {
            *resource_id = (char*)(path + 27);
            return REDFISH_RESOURCE_MANAGERS_ETHERNET_INTERFACE;
        }

        if ((strlen(path) == 50) && (strncmp(&path[46], "eth0", 4) == 0)) {
            *resource_id = (char*)(path + 46);
            return REDFISH_RESOURCE_MANAGERS_ETHERNET_INTERFACE_ETH0;
        }
    }

    if (strncmp(path, "redfish/v1/AccountService", 25) == 0) {
        if (strlen(path) == 25) {
            return REDFISH_RESOURCE_ACCOUNTSERVICE;
        }
        if (path[25] == '/' && strncmp(&path[26], "Accounts", 8) == 0) {
            if (strlen(path) == 34) {
                // Collection: /redfish/v1/AccountService/Accounts
                return REDFISH_RESOURCE_ACCOUNTSERVICE_ACCOUNTS;
            }
            if (path[34] == '/') {
                // Individual account: /redfish/v1/AccountService/Accounts/{id}
                *resource_id = (char*)(path + 35);
                return REDFISH_RESOURCE_ACCOUNTSERVICE_ACCOUNT;
            }
        }
        if (path[25] == '/' && strncmp(&path[26], "Roles", 5) == 0) {
            if (strlen(path) == 31) {
                return REDFISH_RESOURCE_ACCOUNTSERVICE_ROLES_COLLECTION;
            }
            if (path[31] == '/') {
                *resource_id = (char*)(path + 32);
                return REDFISH_RESOURCE_ACCOUNTSERVICE_ROLE;
            }
        }
    }

    if (strncmp(path, "redfish/v1/SessionService", 25) == 0) {
        if (strlen(path) == 25) {
            return REDFISH_RESOURCE_SESSIONSERVICE;
        }
        // Check Members alias before general Sessions check
        if (path[25] == '/' && strncmp(&path[26], "Sessions/Members", 16) == 0) {
            return REDFISH_RESOURCE_SESSIONSERVICE_SESSIONS_MEMBERS;
        }
        if (path[25] == '/' && strncmp(&path[26], "Sessions", 8) == 0) {
            *resource_id = (char*)(path + 26);
            return REDFISH_RESOURCE_SESSIONSERVICE_SESSIONS;
        }
    }

    if (strncmp(path, "redfish/v1/ThermalEquipment", 27) == 0) {
        if (strlen(path) == 27) {
            return REDFISH_RESOURCE_THERMALEQUIPMENT_COLLECTION;
        }
        if (path[27] == '/') {
            // Check IOBoard Read action path EARLY to avoid member fallback:
            // /redfish/v1/ThermalEquipment/CDUs/{cdu}/Oem/Kenmec/IOBoards/{member}/Actions/Oem/KenmecIOBoard.Read
            {
                const char *base = path; // full path
                const char *prefix = "redfish/v1/ThermalEquipment/CDUs/";
                if (strncmp(base, prefix, strlen(prefix)) == 0) {
                    const char *p = base + strlen(prefix); // points at {cdu}/...
                    const char *slash = strchr(p, '/');
                    if (slash) {
                        size_t id_len = (size_t)(slash - p);
                        if (id_len > 0 && id_len < 32) {
                            static char cdu_id[32];
                            strncpy(cdu_id, p, id_len); cdu_id[id_len] = '\0';
                            const char *rest = slash; // starts with "/Oem..."
                            const char *mid = "/Oem/Kenmec/IOBoards/";
                            const char *tail = "/Actions/Oem/KenmecIOBoard.Read";
                            const char *m = strstr(rest, mid);
                            const char *t = strstr(rest, tail);
                            if (m == rest && t && t[strlen(tail)] == '\0' && m < t) {
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM_IOBOARD_ACTION_READ;
                            }
                        }
                    }
                }
            }

            // Check IOBoard Write action path EARLY: /redfish/v1/ThermalEquipment/CDUs/{cdu}/Oem/Kenmec/IOBoards/{member}/Actions/Oem/KenmecIOBoard.Write
            {
                const char *base = path;
                const char *prefix = "redfish/v1/ThermalEquipment/CDUs/";
                if (strncmp(base, prefix, strlen(prefix)) == 0) {
                    const char *p = base + strlen(prefix);
                    const char *slash = strchr(p, '/');
                    if (slash) {
                        size_t id_len = (size_t)(slash - p);
                        if (id_len > 0 && id_len < 32) {
                            static char cdu_id[32];
                            strncpy(cdu_id, p, id_len); cdu_id[id_len] = '\0';
                            const char *rest = slash;
                            const char *mid = "/Oem/Kenmec/IOBoards/";
                            const char *tail = "/Actions/Oem/KenmecIOBoard.Write";
                            const char *m = strstr(rest, mid);
                            const char *t = strstr(rest, tail);
                            if (m == rest && t && t[strlen(tail)] == '\0' && m < t) {
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM_IOBOARD_ACTION_WRITE;
                            }
                        }
                    }
                }
            }

            // Check exact IOBoards collection path first: /redfish/v1/ThermalEquipment/CDUs/{id}/Oem/Kenmec/IOBoards
            {
                const char *suffix = "/Oem/Kenmec/IOBoards";
                char *p = strstr((char*)path + 28, suffix);
                if (p && p[strlen(suffix)] == '\0') {
                    char *cdu_path = (char*)(path + 28);
                    if (strncmp(cdu_path, "CDUs/", 5) == 0) {
                        static char cdu_id[32];
                        char *slash_pos = strchr(cdu_path + 5, '/');
                        if (slash_pos) {
                            size_t len = slash_pos - (cdu_path + 5);
                            if (len < sizeof(cdu_id)) {
                                strncpy(cdu_id, cdu_path + 5, len);
                                cdu_id[len] = '\0';
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM_IOBOARDS;
                            }
                        }
                    }
                }
            }

            // /redfish/v1/ThermalEquipment/CDUs/{cdu}/Oem/Kenmec/ControlLogics/{member}/Actions/Oem/ControlLogic.Read
            {
                const char *base = path; // full path
                const char *prefix = "redfish/v1/ThermalEquipment/CDUs/";
                // debug(tag, "base: %s, prefix: %s", base, prefix);
                if (strncmp(base, prefix, strlen(prefix)) == 0) {
                    // debug(tag, "base: %s, prefix: %s", base, prefix);
                    const char *p = base + strlen(prefix); // points at {cdu}/...
                    const char *slash = strchr(p, '/');
                    if (slash) {
                        // debug(tag, "slash: %s", slash);
                        size_t id_len = (size_t)(slash - p);
                        if (id_len > 0 && id_len < 32) {
                            static char cdu_id[32];
                            strncpy(cdu_id, p, id_len); cdu_id[id_len] = '\0';
                            const char *rest = slash; // starts with "/Oem..."
                            const char *mid = "/Oem/Kenmec/ControlLogics/";
                            const char *tail = "/Actions/Oem/ControlLogic.Read";
                            const char *m = strstr(rest, mid);
                            const char *t = strstr(rest, tail);
                            // debug(tag, "m: %s, t: %s", m, t);
                            if (m == rest && t && t[strlen(tail)] == '\0' && m < t) {
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS_ACTION_READ;
                            }
                        }
                    }
                }
            }

            // /redfish/v1/ThermalEquipment/CDUs/{cdu}/Oem/Kenmec/ControlLogics/{member}/Actions/Oem/ControlLogic.Write
            {
                const char *base = path; // full path
                const char *prefix = "redfish/v1/ThermalEquipment/CDUs/";
                if (strncmp(base, prefix, strlen(prefix)) == 0) {
                    const char *p = base + strlen(prefix); // points at {cdu}/...
                    const char *slash = strchr(p, '/');
                    if (slash) {
                        size_t id_len = (size_t)(slash - p);
                        if (id_len > 0 && id_len < 32) {
                            static char cdu_id[32];
                            strncpy(cdu_id, p, id_len); cdu_id[id_len] = '\0';
                            const char *rest = slash; // starts with "/Oem..."
                            const char *mid = "/Oem/Kenmec/ControlLogics/";
                            const char *tail = "/Actions/Oem/ControlLogic.Write";
                            const char *m = strstr(rest, mid);
                            const char *t = strstr(rest, tail);
                            if (m == rest && t && t[strlen(tail)] == '\0' && m < t) {
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS_ACTION_WRITE;
                            }
                        }
                    }
                }
            }            
     
            // Check ControlLogic member path: /redfish/v1/ThermalEquipment/CDUs/{id}/Oem/Kenmec/ControlLogics
            {
                const char *suffix = "/Oem/Kenmec/ControlLogics";
                char *p = strstr((char*)path + 28, suffix);
                if (p && p[strlen(suffix)] == '\0') {
                    char *cdu_path = (char*)(path + 28);
                    if (strncmp(cdu_path, "CDUs/", 5) == 0) {
                        static char cdu_id[32];
                        char *slash_pos = strchr(cdu_path + 5, '/');
                        if (slash_pos) {
                            size_t len = slash_pos - (cdu_path + 5);
                            if (len < sizeof(cdu_id)) {
                                strncpy(cdu_id, cdu_path + 5, len);
                                cdu_id[len] = '\0';
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS;
                            }
                        }
                    }
                }
            }

            // Check ControlLogic member path: /redfish/v1/ThermalEquipment/CDUs/{id}/Oem/Kenmec/ControlLogics/{member}
            {
                const char *anchor = "/Oem/Kenmec/ControlLogics/";
                char *p = strstr((char*)path + 28, anchor);
                if (p && p[strlen(anchor)] != '\0') {
                    // Extract CDU ID
                    char *cdu_path = (char*)(path + 28);
                    if (strncmp(cdu_path, "CDUs/", 5) == 0) {
                        static char cdu_id[32];
                        char *slash_pos = strchr(cdu_path + 5, '/');
                        if (slash_pos) {
                            size_t len = slash_pos - (cdu_path + 5);
                            if (len < sizeof(cdu_id)) {
                                strncpy(cdu_id, cdu_path + 5, len);
                                cdu_id[len] = '\0';
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM_CONTROL_LOGICS_MEMBER;
                            }
                        }
                    }
                }
            }
     
            // Check IOBoard member path: /redfish/v1/ThermalEquipment/CDUs/{id}/Oem/Kenmec/IOBoards/{member}
            {
                const char *anchor = "/Oem/Kenmec/IOBoards/";
                char *p = strstr((char*)path + 28, anchor);
                if (p && p[strlen(anchor)] != '\0') {
                    // Extract CDU ID
                    char *cdu_path = (char*)(path + 28);
                    if (strncmp(cdu_path, "CDUs/", 5) == 0) {
                        static char cdu_id[32];
                        char *slash_pos = strchr(cdu_path + 5, '/');
                        if (slash_pos) {
                            size_t len = slash_pos - (cdu_path + 5);
                            if (len < sizeof(cdu_id)) {
                                strncpy(cdu_id, cdu_path + 5, len);
                                cdu_id[len] = '\0';
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM_IOBOARD_MEMBER;
                            }
                        }
                    }
                }
            }

            // Check path: /redfish/v1/ThermalEquipment/CDUs/{id}/Oem/Kenmec/Config.Read
            {
                const char *anchor = "/Oem/Kenmec/Config.Read";
                char *p = strstr((char*)path + 28, anchor);
                if (p && p[strlen(anchor)] == '\0') {
                    // Extract CDU ID
                    char *cdu_path = (char*)(path + 28);
                    if (strncmp(cdu_path, "CDUs/", 5) == 0) {
                        static char cdu_id[32];
                        char *slash_pos = strchr(cdu_path + 5, '/');
                        if (slash_pos) {
                            size_t len = slash_pos - (cdu_path + 5);
                            if (len < sizeof(cdu_id)) {
                                strncpy(cdu_id, cdu_path + 5, len);
                                cdu_id[len] = '\0';
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM_KENMEC_CONFIG_READ;
                            }
                        }
                    }
                }
            }

            // Check path: /redfish/v1/ThermalEquipment/CDUs/{id}/Oem/Kenmec/Config.Write
            {
                const char *anchor = "/Oem/Kenmec/Config.Write";
                char *p = strstr((char*)path + 28, anchor);
                if (p && p[strlen(anchor)] == '\0') {
                    // Extract CDU ID
                    char *cdu_path = (char*)(path + 28);
                    if (strncmp(cdu_path, "CDUs/", 5) == 0) {
                        static char cdu_id[32];
                        char *slash_pos = strchr(cdu_path + 5, '/');
                        if (slash_pos) {
                            size_t len = slash_pos - (cdu_path + 5);
                            if (len < sizeof(cdu_id)) {
                                strncpy(cdu_id, cdu_path + 5, len);
                                cdu_id[len] = '\0';
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM_KENMEC_CONFIG_WRITE;
                            }
                        }
                    }
                }
            }            

            // Check exact Kenmec OEM container path: /redfish/v1/ThermalEquipment/CDUs/{id}/Oem/Kenmec
            {
                const char *suffix = "/Oem/Kenmec";
                char *p = strstr((char*)path + 28, suffix);
                if (p && p[strlen(suffix)] == '\0') {
                    char *cdu_path = (char*)(path + 28);
                    if (strncmp(cdu_path, "CDUs/", 5) == 0) {
                        static char cdu_id[32];
                        char *slash_pos = strchr(cdu_path + 5, '/');
                        if (slash_pos) {
                            size_t len = slash_pos - (cdu_path + 5);
                            if (len < sizeof(cdu_id)) {
                                strncpy(cdu_id, cdu_path + 5, len);
                                cdu_id[len] = '\0';
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM_KENMEC;
                            }
                        }
                    }
                }
            }

            // Check IOBoard Read action path: /redfish/v1/ThermalEquipment/CDUs/{cdu}/Oem/Kenmec/IOBoards/{member}/Actions/Oem/KenmecIOBoard.Read
            {
                const char *base = path; // full path
                const char *prefix = "redfish/v1/ThermalEquipment/CDUs/";
                if (strncmp(base, prefix, strlen(prefix)) == 0) {
                    const char *p = base + strlen(prefix); // points at {cdu}/...
                    const char *slash = strchr(p, '/');
                    if (slash) {
                        size_t id_len = (size_t)(slash - p);
                        if (id_len > 0 && id_len < 32) {
                            static char cdu_id[32];
                            strncpy(cdu_id, p, id_len); cdu_id[id_len] = '\0';
                            const char *rest = slash;
                            const char *expected_tail = "/Oem/Kenmec/IOBoards/";
                            const char *t = strstr(rest, expected_tail);
                            if (t == rest && strstr(rest, "/Actions/Oem/KenmecIOBoard.Read") != NULL) {
                                const char *action_tail = "/Actions/Oem/KenmecIOBoard.Read";
                                const char *end = strstr(rest, action_tail);
                                if (end && end[strlen(action_tail)] == '\0') {
                                    *resource_id = cdu_id;
                                    return REDFISH_RESOURCE_CDU_OEM_IOBOARD_ACTION_READ;
                                }
                            }
                        }
                    }
                }
            }

            // Check exact OEM container path: /redfish/v1/ThermalEquipment/CDUs/{id}/Oem
            {
                const char *suffix = "/Oem";
                char *p = strstr((char*)path + 28, suffix);
                if (p && p[strlen(suffix)] == '\0') {
                    char *cdu_path = (char*)(path + 28); // "CDUs/1/Oem"
                    if (strncmp(cdu_path, "CDUs/", 5) == 0) {
                        static char cdu_id[32];
                        char *slash_pos = strchr(cdu_path + 5, '/');
                        if (slash_pos) {
                            size_t len = slash_pos - (cdu_path + 5);
                            if (len < sizeof(cdu_id)) {
                                strncpy(cdu_id, cdu_path + 5, len);
                                cdu_id[len] = '\0';
                                *resource_id = cdu_id;
                                return REDFISH_RESOURCE_CDU_OEM;
                            }
                        }
                    }
                }
            }

            // Fallback: treat the remainder after /redfish/v1/ThermalEquipment/ as thermal equipment identifier
            *resource_id = (char*)(path + 28);
            return REDFISH_RESOURCE_THERMALEQUIPMENT;
        }
    }

    return REDFISH_RESOURCE_UNKNOWN;
}

const char* get_http_status_text(int status_code) {
    switch (status_code) {
        case HTTP_OK: return "OK";
        case HTTP_ACCEPTED: return "Accepted";
        case HTTP_CREATED: return "Created";
        case HTTP_NO_CONTENT: return "No Content";
        case HTTP_TEMPORARY_REDIRECT: return "Temporary Redirect";
        case HTTP_BAD_REQUEST: return "Bad Request";
        case HTTP_UNAUTHORIZED: return "Unauthorized";
        case HTTP_FORBIDDEN: return "Forbidden";
        case HTTP_NOT_FOUND: return "Not Found";
        case HTTP_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTP_PRECONDITION_FAILED: return "Precondition Failed";
        case HTTP_INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HTTP_NOT_IMPLEMENTED: return "Not Implemented";
        default: return "Unknown";
    }
}

void add_cors_headers(http_response_t *response) {
    if (response->header_count >= MAX_HEADERS) {
        return;
    }
    
    strcpy(response->headers[response->header_count][0], "Access-Control-Allow-Origin");
    strcpy(response->headers[response->header_count][1], "*");
    response->header_count++;
    
    if (response->header_count >= MAX_HEADERS) {
        return;
    }
    
    strcpy(response->headers[response->header_count][0], "Access-Control-Allow-Methods");
    strcpy(response->headers[response->header_count][1], "GET, POST, PUT, PATCH, DELETE");
    response->header_count++;
    
    if (response->header_count >= MAX_HEADERS) {
        return;
    }
    
    strcpy(response->headers[response->header_count][0], "Access-Control-Allow-Headers");
    strcpy(response->headers[response->header_count][1], "Content-Type, Authorization");
    response->header_count++;

    if (response->header_count >= MAX_HEADERS) {
        return;
    }

    // Ensure Cache-Control header is present (validator expects it on service root)
    strcpy(response->headers[response->header_count][0], "Cache-Control");
    strcpy(response->headers[response->header_count][1], "no-store");
    response->header_count++;
}

// HTTP Server Implementation
static int g_http_server_fd = -1;
static struct sockaddr_in g_http_server_addr;

int http_server_init(int port) {
    // Create socket
    g_http_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_http_server_fd < 0) {
        error(tag, "Failed to create HTTP server socket");
        return ERROR_NETWORK;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(g_http_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error(tag, "Failed to set HTTP socket options");
        close(g_http_server_fd);
        return ERROR_NETWORK;
    }
    
    // Bind socket
    memset(&g_http_server_addr, 0, sizeof(g_http_server_addr));
    g_http_server_addr.sin_family = AF_INET;
    g_http_server_addr.sin_addr.s_addr = INADDR_ANY;
    g_http_server_addr.sin_port = htons(port);
    
    // Retry binding with a small delay if it fails initially
    int bind_attempts = 0;
    const int max_bind_attempts = 30;
    const int bind_retry_delay_ms = 1000; // 1 second
    
    while (bind_attempts < max_bind_attempts) {
        if (bind(g_http_server_fd, (struct sockaddr*)&g_http_server_addr, sizeof(g_http_server_addr)) == 0) {
            break; // Bind successful
        }
        
        bind_attempts++;
        if (bind_attempts < max_bind_attempts) {
            warn(tag, "Failed to bind HTTP server socket to port %d (attempt %d/%d), retrying in %d ms", 
                 port, bind_attempts, max_bind_attempts, bind_retry_delay_ms);
            usleep(bind_retry_delay_ms * 1000); // Convert ms to microseconds
        }
    }
    
    // if (bind(g_http_server_fd, (struct sockaddr*)&g_http_server_addr, sizeof(g_http_server_addr)) < 0) {
    //     error(tag, "Failed to bind HTTP server socket to port %d", port);
    //     close(g_http_server_fd);
    //     return ERROR_NETWORK;
    // }
    
    // Listen for connections
    if (listen(g_http_server_fd, MAX_CLIENTS) < 0) {
        error(tag, "Failed to listen on HTTP server socket");
        close(g_http_server_fd);
        return ERROR_NETWORK;
    }
    
    debug(tag, "HTTP server initialized on port %d", port);
    return SUCCESS;
}

int http_server_get_fd(void) {
    return g_http_server_fd;
}

void http_server_cleanup(void) {
    if (g_http_server_fd >= 0) {
        close(g_http_server_fd);
        g_http_server_fd = -1;
        debug(tag, "HTTP server cleanup completed");
    }
}

int handle_http_client_connection(int client_fd) {
    char request_buffer[BUFFER_SIZE];
    char response_buffer[BUFFER_SIZE];
    http_request_t request;
    http_response_t response;
    
    // Initialize buffers to zero
    memset(request_buffer, 0, sizeof(request_buffer));
    memset(response_buffer, 0, sizeof(response_buffer));
    
    debug(tag, "Handling HTTP client connection on fd %d", client_fd);

    // Read request from client
    int bytes_read = recv(client_fd, request_buffer, sizeof(request_buffer) - 1, 0);
    if (bytes_read <= 0) {
        error(tag, "Failed to read HTTP request from client");
        close(client_fd);
        return ERROR_NETWORK;
    }
    
    request_buffer[bytes_read] = '\0';

    // debug(tag, "Received HTTP request:\n%s\n", request_buffer);

    // Parse HTTP request
    int ret = parse_http_request(request_buffer, &request, 0);  // HTTP
    if (ret != SUCCESS) {
        error(tag, "Failed to parse HTTP request");
        close(client_fd);
        return ret;
    }

    // Ensure full body is read based on Content-Length (HTTP)
    int expected_len = 0;
    for (int i = 0; i < request.header_count; i++) {
        if (strcasecmp(request.headers[i][0], "Content-Length") == 0) {
            expected_len = atoi(request.headers[i][1]);
            break; 
        }
    }
    // Stream large multipart uploads to a temp file
    int is_upload = 0;
    if (strcmp(request.method, HTTP_METHOD_POST) == 0 && strstr(request.path, "/UpdateFirmwareMultipart") != NULL) {
        is_upload = 1;
    }

    // Set a 1-second recv timeout to avoid hanging reads
    {
        struct timeval rcv_to;
        rcv_to.tv_sec = 5;
        rcv_to.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &rcv_to, sizeof(rcv_to));
    }

    if (!is_upload) {
        if (expected_len > request.content_length && expected_len < (int)sizeof(request.body)) {
            int remaining = expected_len - request.content_length;
            int offset = request.content_length;
            while (remaining > 0) {
                int n = recv(client_fd, request.body + offset, remaining, 0);
                if (n <= 0) {
                    printf("Failed to read remaining HTTP body bytes\n");
                    close(client_fd);
                    return ERROR_NETWORK;
                }
                offset += n;
                remaining -= n;
            }
            request.body[expected_len] = '\0';
            request.content_length = expected_len;
        }
    } else {
        // Create fixed temp file for firmware upload
        system_firmware_file_path(request.upload_tmp_path);

        int tmp_fd = open(request.upload_tmp_path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (tmp_fd < 0) {
            printf("Failed to create temp upload file\n");
            close(client_fd);
            return ERROR_NETWORK;
        }
        // Write buffered bytes
        if (request.content_length > 0) {
            if (write(tmp_fd, request.body, (size_t)request.content_length) != request.content_length) {
                printf("Failed to write initial upload bytes\n");
                close(tmp_fd);
                close(client_fd);
                return ERROR_NETWORK;
            }
        }
        int remaining = expected_len - request.content_length;
        char chunk[8192];
        while (remaining > 0) {
            int to_read = remaining > (int)sizeof(chunk) ? (int)sizeof(chunk) : remaining;
            int n = recv(client_fd, chunk, to_read, 0);
            if (n <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("recv timeout while reading upload bytes\n");
                } else {
                    printf("Failed to read remaining upload bytes\n");
                }
                close(tmp_fd);
                close(client_fd);
                return ERROR_NETWORK;
            }
            if (write(tmp_fd, chunk, (size_t)n) != n) {
                printf("Failed to write upload chunk\n");
                close(tmp_fd);
                close(client_fd);
                return ERROR_NETWORK;
            }
            remaining -= n;
        }
        fsync(tmp_fd);
        close(tmp_fd);
    }

    debug(tag, "************");
    debug(tag, "Method: %s", request.method);
    debug(tag, "Path: %s", request.path);
    for (int i = 0; i < request.header_count; i++) {
        debug(tag, "Header: [%s] = [%s]", request.headers[i][0], request.headers[i][1]);
    }
    debug(tag, "Body: %s", request.body);
    debug(tag, "************");

    // Process Redfish request
    ret = process_redfish_request(&request, &response);
    if (ret != SUCCESS) {
        error(tag, "Failed to process Redfish request");
        close(client_fd);
        return ret;
    }

    // Generate HTTP response
    generate_http_response(&response, response_buffer, sizeof(response_buffer));

    debug(tag, "Sending HTTP response:\n%s\n", response_buffer);

    // Send response to client
    // Send response to client with proper size handling
    size_t response_len = strlen(response_buffer);
    size_t total_sent = 0;
    
    while (total_sent < response_len) {
        int bytes_to_send = (response_len - total_sent > 4096) ? 4096 : (int)(response_len - total_sent);
        int bytes_sent = send(client_fd, response_buffer + total_sent, bytes_to_send, 0);
        
        if (bytes_sent < 0) {
            error(tag, "Failed to send HTTP response to client");
            close(client_fd);
            return ERROR_NETWORK;
        }
        
        total_sent += bytes_sent;
    }
    // int bytes_written = send(client_fd, response_buffer, strlen(response_buffer), 0);
    // if (bytes_written < 0) {
    //     printf("Failed to write HTTP response to client\n");
    //     close(client_fd);
    //     return ERROR_NETWORK;
    // } 

    debug(tag, "Successfully sent %zu bytes to client", total_sent);

    // Close client connection
    close(client_fd);

    // Execute post action if specified
    if (response.post_action != LABEL_POST_ACTION_NONE) {
        printf("Executing post action %d after response sent\n", response.post_action);
        redfish_server_post_action(response.post_action, "HTTP client");
    }
    
    return SUCCESS;
}

int https_server_init(int port, const char *cert_file, const char *key_file, const char *client_ca_file) {
    // This function will be implemented to initialize the HTTPS server
    // For now, it will call the existing TLS server initialization
    debug(tag, "HTTPS server initialization delegated to TLS server");
    (void)port;
    (void)cert_file;
    (void)key_file;
    (void)client_ca_file;

    return SUCCESS;
}

void https_server_cleanup(void) {
    debug(tag, "HTTPS server cleanup completed");
}

int handle_https_client_connection(int client_fd) {
    // This function will handle HTTPS connections using the existing TLS implementation
    // For now, it will call the existing handle_client_connection function
    return handle_client_connection(client_fd);
}

int redfish_server_post_action(label_post_action_t action, const char *context) {
    if (!context) {
        printf("[%s] Error: context parameter is NULL\n", __FUNCTION__);
        return ERROR_INVALID_PARAM;
    }

    printf("[%s] Executing post action %d for context: %s\n", __FUNCTION__, action, context);

    switch (action) {
        case LABEL_POST_ACTION_NONE:
            printf("[%s] No post action required\n", __FUNCTION__);
            break;

        case LABEL_POST_ACTION_FORCE_RESTART:
            printf("[%s] Executing force restart for context: %s\n", __FUNCTION__, context);
            system_reset();
            printf("[%s] Force restart initiated for %s\n", __FUNCTION__, context);
            break;

        default:
            printf("[%s] Unknown post action: %d\n", __FUNCTION__, action);
            return ERROR_INVALID_PARAM;
    }

    return SUCCESS;
}
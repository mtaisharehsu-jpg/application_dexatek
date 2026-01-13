#include <mbedtls/base64.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <cJSON.h>
#include "dexatek/main_application/include/application_common.h"
#include "dexatek/main_application/include/utilities/net_utilities.h"
#include "redfish_client_info_handle.h"
#include "redfish_resources.h"
#include "redfish_server.h"
#include "redfish_crypto.h"
#include "default_json.h"
#include "redfish_hid_bridge.h"
#include "kenmec/main_application/kenmec_config.h"
#include "kenmec/main_application/control_logic/control_logic_manager.h"

// static const char *tag = "redfish_resources";
static int g_securitypolicy_applytime_onreset = 0; // set by POST to annotate next GET

#define INTERFACE_ID "1"

// Structure for delayed network configuration
typedef struct {
    NetUtilitiesEthernetConfig config;
    int delay_seconds;
} delayed_network_config_t;

// static int encode_base64(const uint8_t *in, size_t in_len, char *out, size_t out_size, size_t *out_len) {
//     size_t olen = 0;
//     int rc = mbedtls_base64_encode((unsigned char*)out, out_size, &olen, (const unsigned char*)in, in_len);
//     if (rc != 0) return ERROR_GENERAL;
//     if (out_len) *out_len = olen;
//     return SUCCESS;
// }

int handle_cdu_oem_ioboard_action_read(const char *cdu_id, const char *member_id, const http_request_t *request, http_response_t *response) {
    if (!cdu_id || !member_id || !request || !response) return ERROR_INVALID_PARAM;

    // int ret = ERROR_GENERAL;

    // Validate CDU and member
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s was not found.\"}}", cdu_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    uint16_t hid_pid_list[32] = {0};
    size_t hid_count = 0;
    if (redfish_hid_device_list_get(hid_pid_list, 32, &hid_count) != SUCCESS) hid_count = 0;
    long port_idx = strtol(member_id, NULL, 10);
    if (port_idx <= 0 || (size_t)port_idx > hid_count) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%s was not found.\"}}", cdu_id, member_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Build success response
    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");

    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddStringToObject(response_json, "@odata.type", "#KenmecIOBoard.v1_0_0.ReadResponse");
    
    char odata_id[256];
    snprintf(odata_id, sizeof(odata_id), "/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%s/Actions/Oem/KenmecIOBoard.Read", cdu_id, member_id);
    cJSON_AddStringToObject(response_json, "@odata.id", odata_id);

    redfish_board_data_append_to_json(port_idx, response_json);
    
    char *json_string = cJSON_Print(response_json);
    if (json_string) {
        strncpy(response->body, json_string, sizeof(response->body) - 1);
        response->body[sizeof(response->body) - 1] = '\0';
        free(json_string);
    }
    cJSON_Delete(response_json);

    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_cdu_oem_ioboard_action_write(const char *cdu_id, const char *member_id, const http_request_t *request, http_response_t *response) {
    if (!cdu_id || !member_id || !request || !response) return ERROR_INVALID_PARAM;

    // Validate CDU and member
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s was not found.\"}}", cdu_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    uint16_t hid_pid_list[32] = {0};
    size_t hid_count = 0;
    if (redfish_hid_device_list_get(hid_pid_list, 32, &hid_count) != SUCCESS) hid_count = 0;
    long port_idx = strtol(member_id, NULL, 10);
    if (port_idx <= 0 || (size_t)port_idx > hid_count) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%s was not found.\"}}", cdu_id, member_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Parse request JSON
    cJSON *root = cJSON_Parse(request->body);
    if (!root) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ActionParameterMissing\",\"message\":\"Invalid JSON.\"}}" );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
    
    cJSON *pid = cJSON_GetObjectItemCaseSensitive(root, "Pid");
    cJSON *timeout = cJSON_GetObjectItemCaseSensitive(root, "TimeoutMs");
    if (!cJSON_IsNumber(pid)) {
        cJSON_Delete(root);
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ActionParameterMissing\",\"message\":\"One or more required parameters are missing.\"}}" );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    uint16_t req_pid = (uint16_t)pid->valuedouble;

    int req_timeout = (timeout && cJSON_IsNumber(timeout)) ? (int)timeout->valuedouble : 1000;

    if (req_pid != hid_pid_list[port_idx - 1]) {
        cJSON_Delete(root);
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ActionParameterValueFormatError\",\"message\":\"Invalid parameter values.\"}}" );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
    
    int ret = redfish_board_write(port_idx, request->body, req_timeout);

    cJSON_Delete(root);
    if (ret != SUCCESS) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, "application/json");
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"HID write failed.\"}}" );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");

    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddStringToObject(response_json, "@odata.type", "#KenmecIOBoard.v1_0_0.WriteResponse");
    
    char odata_id[512];
    snprintf(odata_id, sizeof(odata_id), 
        "/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%s/Actions/Oem/KenmecIOBoard.Write",
        cdu_id, member_id);
    cJSON_AddStringToObject(response_json, "@odata.id", odata_id);
    cJSON_AddNumberToObject(response_json, "Pid", req_pid);
    cJSON_AddNumberToObject(response_json, "Port", port_idx);
    cJSON_AddStringToObject(response_json, "Status", "Success");
    
    char *json_string = cJSON_Print(response_json);
    if (json_string) {
        strncpy(response->body, json_string, sizeof(response->body) - 1);
        response->body[sizeof(response->body) - 1] = '\0';
        free(json_string);
    }
    cJSON_Delete(response_json);

    response->content_length = strlen(response->body);

    return SUCCESS;
}
// Helper to build Role resource JSON
static void write_role_json(char *buf, size_t buflen, const char *role_id,
                            const char *is_predefined,
                            const char *assigned[], size_t assigned_count)
{
    char assigned_buf[512] = {0};
    size_t off = 0;
    for (size_t i = 0; i < assigned_count; ++i) {
        int n = snprintf(assigned_buf + off, sizeof(assigned_buf) - off,
                         "%s\"%s\"", (i == 0 ? "" : ","), assigned[i]);
        if (n < 0) break;
        off += (size_t)n;
        if (off >= sizeof(assigned_buf)) break;
    }

    snprintf(buf, buflen,
        "{"
          "\"@odata.type\":\"#Role.v1_3_3.Role\"," 
          "\"@odata.id\":\"/redfish/v1/AccountService/Roles/%s\"," 
          "\"Id\":\"%s\"," 
          "\"Name\":\"%s Role\"," 
          "\"RoleId\":\"%s\","
          "\"IsPredefined\":%s,"
          "\"AssignedPrivileges\":[%s],"
          "\"Oem\":{}"
        "}",
        role_id, role_id, role_id, role_id, is_predefined, assigned_buf);
}

int handle_roles_collection(http_response_t *response)
{
    if (!response) return ERROR_INVALID_PARAM;

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    snprintf(response->body, sizeof(response->body),
        "{"
          "\"@odata.type\":\"#RoleCollection.RoleCollection\"," 
          "\"@odata.id\":\"/redfish/v1/AccountService/Roles\"," 
          "\"Name\":\"Role Collection\"," 
          "\"Members\":[{\"@odata.id\":\"/redfish/v1/AccountService/Roles/Administrator\"},"
                         "{\"@odata.id\":\"/redfish/v1/AccountService/Roles/Operator\"},"
                         "{\"@odata.id\":\"/redfish/v1/AccountService/Roles/ReadOnly\"}],"
          "\"Members@odata.count\":3"
        "}");
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_role_member(const char *role_id, http_response_t *response)
{
    if (!role_id || !response) return ERROR_INVALID_PARAM;

    char buf[1024];
    const char *admin_privs[] = {"Login","ConfigureManager","ConfigureUsers","ConfigureComponents","ConfigureSelf"};
    const char *oper_privs[]  = {"Login","ConfigureSelf","ConfigureComponents"};
    const char *ro_privs[]    = {"Login","ConfigureSelf"};

    if (strcmp(role_id, "Administrator") == 0) {
        write_role_json(buf, sizeof(buf), "Administrator", "true",
                        admin_privs, sizeof(admin_privs)/sizeof(admin_privs[0]));
    } else if (strcmp(role_id, "Operator") == 0) {
        write_role_json(buf, sizeof(buf), "Operator", "true",
                        oper_privs, sizeof(oper_privs)/sizeof(oper_privs[0]));
    } else if (strcmp(role_id, "ReadOnly") == 0) {
        write_role_json(buf, sizeof(buf), "ReadOnly", "true",
                        ro_privs, sizeof(ro_privs)/sizeof(ro_privs[0]));
    } else {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
                 "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"Role not found\"}}"
        );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    strcpy(response->body, buf);
    response->content_length = strlen(response->body);
    return SUCCESS;
}int handle_cdu_oem_control_logics_action_read(const char *cdu_id, const char *member_id, const http_request_t *request, http_response_t *response) {
    if (!cdu_id || !member_id || !request || !response) return ERROR_INVALID_PARAM;

    // int ret = ERROR_GENERAL;

    // Validate CDU and member
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s was not found.\"}}", cdu_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // check control logic count
    uint16_t control_logic_count = control_logic_manager_number_of_control_logics();
    long control_logic_idx = strtol(member_id, NULL, 10);
    if (control_logic_idx <= 0 || (size_t)control_logic_idx > control_logic_count) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics/%s was not found.\"}}", cdu_id, member_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Build success response
    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");

    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddStringToObject(response_json, "@odata.type", "#KenmecControlLogic.v1_0_0.ReadResponse");
    
    char odata_id[256];
    snprintf(odata_id, sizeof(odata_id), "/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics/%s/Actions/Oem/ControlLogic.Read", cdu_id, member_id);
    cJSON_AddStringToObject(response_json, "@odata.id", odata_id);
    
    redfish_control_logic_data_append_to_json(control_logic_idx, response_json);

    char *json_string = cJSON_Print(response_json);
    if (json_string) {
        strncpy(response->body, json_string, sizeof(response->body) - 1);
        response->body[sizeof(response->body) - 1] = '\0';
        free(json_string);
    }
    cJSON_Delete(response_json);
    
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_cdu_oem_control_logics_action_write(const char *cdu_id, const char *member_id, const http_request_t *request, http_response_t *response) {
    if (!cdu_id || !member_id || !request || !response) return ERROR_INVALID_PARAM;

    // Validate CDU and member
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s was not found.\"}}", cdu_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // check control logic count
    uint16_t control_logic_count = control_logic_manager_number_of_control_logics();
    long control_logic_idx = strtol(member_id, NULL, 10);
    if (control_logic_idx <= 0 || (size_t)control_logic_idx > control_logic_count) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics/%s was not found.\"}}", cdu_id, member_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Parse request JSON
    cJSON *root = cJSON_Parse(request->body);
    if (!root) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ActionParameterMissing\",\"message\":\"Invalid JSON.\"}}" );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
    
    cJSON *timeout = cJSON_GetObjectItemCaseSensitive(root, "TimeoutMs");

    int req_timeout = (timeout && cJSON_IsNumber(timeout)) ? (int)timeout->valuedouble : 1000;

    // debug(tag, "control_logic_idx: %d, req_timeout: %d", control_logic_idx, req_timeout);
    // debug(tag, "request->body: %s", request->body);
    int ret = redfish_control_logic_write(control_logic_idx, request->body, req_timeout);

    cJSON_Delete(root);
    if (ret != SUCCESS) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, "application/json");
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"Control logic write failed.\"}}" );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");

    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddStringToObject(response_json, "@odata.type", "#KenmecIOBoard.v1_0_0.WriteResponse");
    
    char odata_id[512];
    snprintf(odata_id, sizeof(odata_id), 
        "/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics/%s/Actions/Oem/ControlLogic.Write",
        cdu_id, member_id);
    cJSON_AddStringToObject(response_json, "@odata.id", odata_id);
    cJSON_AddNumberToObject(response_json, "ControlLogicIdx", control_logic_idx);
    cJSON_AddStringToObject(response_json, "Status", "Success");
    
    
    char *json_string = cJSON_Print(response_json);
    if (json_string) {
        strncpy(response->body, json_string, sizeof(response->body) - 1);
        response->body[sizeof(response->body) - 1] = '\0';
        free(json_string);
    }
    cJSON_Delete(response_json);

    response->content_length = strlen(response->body);

    return SUCCESS;
}

/************************************************************/

// Thread function for delayed network configuration
void* delayed_network_config_thread(void* arg) {
    delayed_network_config_t* delayed_config = (delayed_network_config_t*)arg;
    
    printf("Delayed network configuration thread started, waiting %d seconds...\n", delayed_config->delay_seconds);
    
    // Sleep for the specified delay
    sleep(delayed_config->delay_seconds);
    
    printf("Applying delayed network configuration...\n");
    int ret = net_ethernet_config_restart(delayed_config->config);
    if (ret != 0) {
        printf("Warning: Failed to restart ethernet configuration (ret=%d)\n", ret);
    } else {
        printf("Successfully applied delayed ethernet configuration\n");
    }
    
    // Free the delayed config structure
    free(delayed_config);
    
    return NULL;
}

// Function to schedule delayed network configuration
void schedule_delayed_network_config(NetUtilitiesEthernetConfig config, int delay_seconds) {
    delayed_network_config_t* delayed_config = malloc(sizeof(delayed_network_config_t));
    if (!delayed_config) {
        printf("Error: Failed to allocate memory for delayed network config\n");
        return;
    }
    
    delayed_config->config = config;
    delayed_config->delay_seconds = delay_seconds;
    
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    int ret = pthread_create(&thread, &attr, delayed_network_config_thread, delayed_config);
    if (ret != 0) {
        printf("Error: Failed to create delayed network config thread\n");
        free(delayed_config);
    } else {
        printf("Delayed network configuration thread created successfully\n");
    }
    
    pthread_attr_destroy(&attr);
}

void extract_func_middle(const char *func_name, char *out, size_t out_size) {
    const char *prefix = "generate_";
    const char *suffix = "_json";

    const char *start = strstr(func_name, prefix);
    if (!start) {
        out[0] = '\0';
        return;
    }
    start += strlen(prefix);  

    const char *end = strstr(start, suffix);
    if (!end) {
        out[0] = '\0';
        return;
    }

    size_t len = end - start;
    if (len >= out_size) len = out_size - 1;

    strncpy(out, start, len);
    out[len] = '\0';
}

void get_json(FILE* fp, json_resource_type_t type)
{
    char info[2048];
    switch(type) {
        case MANAGER_NONE:
            return;

        case MANAGER_KENMEC:
            set_default_kenmec_json(info);
            break;

        case MANAGER_ETHERNET_INTERFACE:
            set_default_ethernet_json(info);
            break;

        case MANAGER_ETHERNET_INTERFACE_ETH0:
            set_default_eth0_json(info);
            break;

        case MANAGER_COLLECTION:
            set_default_manager_json(info);
            break;
    }

    fwrite(info, 1, strlen(info), fp);
    fflush(fp);
}

char* open_json_file(const char *file_name, json_resource_type_t type)
{
    const char *dir = "redfish_resource_json";
    char path[512] = {0};
    char *buffer = NULL;
    FILE *fp = NULL;
    struct stat st = {0};
    
    // Validate input parameters
    if (!file_name) {
        printf("Error: file_name parameter is NULL\n");
        return NULL;
    }
    
    // Create directory if it doesn't exist
    if (stat(dir, &st) == -1) {
        printf("JSON folder not found, creating directory: %s\n", dir);
        if (mkdir(dir, 0755) != 0) {
            perror("Failed to create JSON folder");
            return NULL;
        }
    }
    
    // Construct file path
    snprintf(path, sizeof(path), "%s/%s.json", dir, file_name);
    
    // Always generate a new JSON file (overwrite if exists)
    printf("Generating JSON file: %s\n", path);
    fp = fopen(path, "w");
    if (!fp) {
        perror("Failed to create/overwrite JSON file");
        return NULL;
    }
    
    // Write JSON content based on type
    get_json(fp, type);
    fclose(fp);
    printf("Generated JSON file with content: %s\n", path);
    
    // Open file for reading
    fp = fopen(path, "r");
    if (!fp) {
        perror("Failed to open JSON file for reading");
        return NULL;
    }
    
    // Get file size
    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("Failed to seek to end of file");
        fclose(fp);
        return NULL;
    }
    
    long filesize = ftell(fp);
    if (filesize < 0) {
        perror("Failed to get file size");
        fclose(fp);
        return NULL;
    }
    
    if (filesize == 0) {
        printf("Warning: JSON file is empty: %s\n", path);
        fclose(fp);
        return NULL;
    }
    
    // Allocate buffer for file content
    buffer = malloc(filesize + 1);
    if (!buffer) {
        perror("Failed to allocate memory for JSON content");
        fclose(fp);
        return NULL;
    }

    rewind(fp);
    
    // Read file content
    size_t read_size = fread(buffer, 1, filesize, fp);
    if (read_size != (size_t)filesize) {
        printf("Warning: Expected to read %ld bytes, but read %zu bytes\n", 
               filesize, read_size);
    }
    
    buffer[read_size] = '\0';
    
    fclose(fp);
    
    return buffer;
}


bool save_json_file(const char *file_name, const char *json_info)
{
    struct stat st = {0};
    const char *dir = "redfish_resource_json";

    if (stat(dir, &st) == -1) {
        printf("redfish_resource_json folder not found, creating...\n");
        if (mkdir(dir, 0755) != 0) {
            perror("mkdir redfish_resource_json folder failed");
            return false;
        }
    }

    char path[512] = {0};
    snprintf(path, sizeof(path), "%s/%s.json", dir, file_name);

    if (stat(path, &st) == -1) {
        printf("%s not found, creating empty file...\n", path);
        FILE *fp = fopen(path, "w");
        if (!fp) {
            perror("create json file failed");
            return false;
        }
        fclose(fp);
    } else {
        printf("%s already exists.\n", path);
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror("open json file for writing failed");
        return false;
    }

    if (fputs(json_info, fp) == EOF) {
        perror("write to json file failed");
        fclose(fp);
        return false;
    }

    fclose(fp);
    printf("Successfully saved JSON to %s\n", path);
    return true;
}

int handle_service_root(http_response_t *response) {
    char *json = generate_service_root_json();
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to generate service root\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int handle_version_root(http_response_t *response) {
    if (!response) return ERROR_INVALID_PARAM;
    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");
    snprintf(response->body, sizeof(response->body),
             "{\"v1\": \"/redfish/v1/\"}");
    response->content_length = strlen(response->body);
    return SUCCESS;
}

// TODO: Implement this according to the services provided.
int handle_odata_service(http_response_t *response) {
    if (!response) return ERROR_INVALID_PARAM;
    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");
    snprintf(response->body, sizeof(response->body),
             "{\"@odata.context\":\"/redfish/v1/$metadata\",\"value\":[]}");
    response->content_length = strlen(response->body);
    return SUCCESS;
}

// TODO: Implement this according to the services provided.
int handle_odata_metadata(http_response_t *response) {
    if (!response) return ERROR_INVALID_PARAM;
    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/xml");
    // Minimal valid XML root for the validator (no schema content for now)
    snprintf(response->body, sizeof(response->body),
             "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
             "<edmx:Edmx xmlns:edmx=\"http://docs.oasis-open.org/odata/ns/edmx\" Version=\"4.0\">"
               "<edmx:Reference Uri=\"http://redfish.dmtf.org/schemas/v1/RedfishExtensions_v1.xml\">"
                 "<edmx:Include Namespace=\"RedfishExtensions.v1_0_0\" Alias=\"Redfish\"/>"
               "</edmx:Reference>"
               "<edmx:Reference Uri=\"http://redfish.dmtf.org/schemas/v1/SessionService_v1.xml\">"
                 "<edmx:Include Namespace=\"SessionService\"/>"
                 "<edmx:Include Namespace=\"SessionService.v1_2_0\"/>"
               "</edmx:Reference>"
               "<edmx:DataServices>"
                 "<Schema xmlns=\"http://docs.oasis-open.org/odata/ns/edm\" Namespace=\"Service\" Alias=\"Service\">"
                   "<EntityContainer Name=\"ServiceContainer\"/>"
                 "</Schema>"
               "</edmx:DataServices>"
             "</edmx:Edmx>");
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_account_service(http_response_t *response) {
    if (!response) return ERROR_INVALID_PARAM;
    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    snprintf(response->body, sizeof(response->body),
        "{"
          "\"@odata.type\":\"#AccountService.v1_18_0.AccountService\"," 
          "\"@odata.id\":\"/redfish/v1/AccountService\"," 
          "\"Id\":\"AccountService\"," 
          "\"Name\":\"Account Service\"," 
          "\"ServiceEnabled\":true,"
          "\"Accounts\":{\"@odata.id\":\"/redfish/v1/AccountService/Accounts\"},"
          "\"Roles\":{\"@odata.id\":\"/redfish/v1/AccountService/Roles\"}"
        "}");
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_accounts_collection(http_response_t *response) {
    if (!response) return ERROR_INVALID_PARAM;
    
    int total_accounts = account_count_get();
    if (total_accounts < 0) {
        total_accounts = 0;
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    
    if (total_accounts == 0) {
        // No accounts, return empty collection
        snprintf(response->body, sizeof(response->body),
            "{"
              "\"@odata.type\":\"#ManagerAccountCollection.ManagerAccountCollection\"," 
              "\"@odata.id\":\"/redfish/v1/AccountService/Accounts\"," 
              "\"Name\":\"Accounts Collection\","
              "\"Members@odata.count\":0,"
              "\"Members\":[]"
            "}");
    } else {
        // Get all accounts
        account_info_t *accounts = calloc(total_accounts, sizeof(account_info_t));
        if (!accounts) {
            // Fallback to empty collection if memory allocation fails
            snprintf(response->body, sizeof(response->body),
                "{"
                  "\"@odata.type\":\"#ManagerAccountCollection.ManagerAccountCollection\"," 
                  "\"@odata.id\":\"/redfish/v1/AccountService/Accounts\"," 
                  "\"Name\":\"Accounts Collection\","
                  "\"Members@odata.count\":0,"
                  "\"Members\":[]"
                "}");
        } else {
            int retrieved = account_list_get(accounts);
            
            // Build members array
            char members_json[4096] = "[";
            for (int i = 0; i < retrieved; i++) {
                if (i > 0) {
                    strcat(members_json, ",");
                }
                char member_entry[256];
                snprintf(member_entry, sizeof(member_entry), 
                    "{\"@odata.id\":\"/redfish/v1/AccountService/Accounts/%d\"}", 
                    accounts[i].id);
                strcat(members_json, member_entry);
            }
            strcat(members_json, "]");
            
            // Build complete response
            snprintf(response->body, sizeof(response->body),
                "{"
                  "\"@odata.type\":\"#ManagerAccountCollection.ManagerAccountCollection\"," 
                  "\"@odata.id\":\"/redfish/v1/AccountService/Accounts\"," 
                  "\"Name\":\"Accounts Collection\","
                  "\"Members@odata.count\":%d,"
                  "\"Members\":%s"
                "}", retrieved, members_json);
            
            free(accounts);
        }
    }
    
    response->content_length = strlen(response->body);
    return SUCCESS;
}

// Helper function to get account information by ID from database
static int get_account_by_id(int account_id, char *username, size_t username_size, char *role, size_t role_size) {
    sqlite3 *db;
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        return -1;
    }

    const char *sql = "SELECT username, role FROM accounts WHERE id = ?;";
    sqlite3_stmt *stmt;
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, account_id);
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *db_username = (const char*)sqlite3_column_text(stmt, 0);
        const char *db_role = (const char*)sqlite3_column_text(stmt, 1);
        
        if (db_username && db_role) {
            strncpy(username, db_username, username_size - 1);
            username[username_size - 1] = '\0';
            strncpy(role, db_role, role_size - 1);
            role[role_size - 1] = '\0';
            
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return 0; // Success
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return -1; // Not found or error
}

int handle_account_member(const char *account_id, http_response_t *response) {
    if (!account_id || !response) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->body, "{\"error\":\"Invalid account ID\"}");
        response->content_length = strlen(response->body);
        return ERROR_INVALID_PARAM;
    }

    int id = atoi(account_id);
    if (id <= 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
            "{"
              "\"error\":{"
                "\"code\":\"Base.1.15.0.ResourceMissingAtURI\","
                "\"message\":\"The resource at the URI /redfish/v1/AccountService/Accounts/%s was not found.\""
              "}"
            "}", account_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    account_info_t account;
    
    if (account_get_by_id(id, &account) != SUCCESS) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
            "{"
              "\"error\":{"
                "\"code\":\"Base.1.15.0.ResourceMissingAtURI\","
                "\"message\":\"The resource at the URI /redfish/v1/AccountService/Accounts/%s was not found.\""
              "}"
            "}", account_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Generate ETag based on account data (simple hash of username + role + id)
    char etag_source[256];
    snprintf(etag_source, sizeof(etag_source), "%s-%s-%d", account.username, account.role, account.id);
    
    // Simple hash for ETag - in production, use a proper hash function
    unsigned int hash = 0;
    for (const char *p = etag_source; *p; p++) {
        hash = hash * 31 + (unsigned char)*p;
    }
    char etag[32];
    snprintf(etag, sizeof(etag), "\"%08x\"", hash);

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    
    // Add ETag header
    if (response->header_count < MAX_HEADERS) {
        strcpy(response->headers[response->header_count][0], "ETag");
        strcpy(response->headers[response->header_count][1], etag);
        response->header_count++;
    }
    
    snprintf(response->body, sizeof(response->body),
        "{"
          "\"@odata.type\":\"#ManagerAccount.v1_12_0.ManagerAccount\","
          "\"@odata.id\":\"/redfish/v1/AccountService/Accounts/%d\","
          "\"Id\":\"%d\","
          "\"Name\":\"User Account\","
          "\"UserName\":\"%s\","
          "\"RoleId\":\"%s\","
          "\"Enabled\":%s,"
          "\"Locked\":%s,"
          "\"AccountTypes\":[\"Redfish\"]"
        "}", account.id, account.id, account.username, account.role, 
        account.enabled ? "true" : "false", account.locked ? "true" : "false");
    
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_account_patch(const char *account_id, const http_request_t *request, http_response_t *response) {
    if (!account_id || !request || !response) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":\"Invalid parameters\"}");
        response->content_length = strlen(response->body);
        return ERROR_INVALID_PARAM;
    }

    int id = atoi(account_id);
    if (id <= 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
            "{"
              "\"error\":{"
                "\"code\":\"Base.1.15.0.ResourceMissingAtURI\","
                "\"message\":\"The resource at the URI /redfish/v1/AccountService/Accounts/%s was not found.\""
              "}"
            "}", account_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Check if account exists
    char username[64] = {0};
    char role[32] = {0};
    if (get_account_by_id(id, username, sizeof(username), role, sizeof(role)) != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
            "{"
              "\"error\":{"
                "\"code\":\"Base.1.15.0.ResourceMissingAtURI\","
                "\"message\":\"The resource at the URI /redfish/v1/AccountService/Accounts/%s was not found.\""
              "}"
            "}", account_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Authorization: enforce that only Administrator can modify other users;
    // non-admin can only modify their own account
    {
        char req_username[128] = {0};
        char req_role[64] = {0};
        if (get_authenticated_identity(request, req_username, sizeof(req_username), req_role, sizeof(req_role)) == SUCCESS) {
            // If not Administrator and target username differs, deny
            if (strcasecmp(req_role, "Administrator") != 0) {
                char target_username[64] = {0};
                char tmp_role[32] = {0};
                if (get_account_by_id(id, target_username, sizeof(target_username), tmp_role, sizeof(tmp_role)) == 0) {
                    if (strcasecmp(req_username, target_username) != 0) {
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
                        return SUCCESS;
                    }
                }
            }
        }
    }

    // Capture If-Match header for potential later validation
    const char *if_match_header = NULL;
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i][0], "If-Match") == 0) {
            if_match_header = request->headers[i][1];
            break;
        }
    }

    // Parse the PATCH request body
    cJSON *json = cJSON_Parse(request->body);
    if (!json) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, 
            "{"
              "\"error\":{"
                "\"code\":\"Base.1.15.0.MalformedJSON\","
                "\"message\":\"The request body submitted was malformed JSON and could not be parsed by the receiving service.\""
              "}"
            "}");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Check property types and categorize them
    cJSON *item;
    bool all_readonly = true;
    bool only_odata_annotations = true;
    cJSON *readonly_props = cJSON_CreateArray();
    cJSON *odata_props = cJSON_CreateArray();
    
    cJSON_ArrayForEach(item, json) {
        const char *property_name = item->string;
        
        // Check if this is an OData annotation (starts with @)
        if (property_name && property_name[0] == '@') {
            cJSON_AddItemToArray(odata_props, cJSON_CreateString(property_name));
        } else {
            only_odata_annotations = false;
            
            // Define read-only properties for ManagerAccount
            if (strcmp(property_name, "Id") == 0 ||
                strcmp(property_name, "Name") == 0 ||
                strcmp(property_name, "Description") == 0 ||
                strcmp(property_name, "UserName") == 0 ||
                strcmp(property_name, "Links") == 0) {
                // Read-only property
                cJSON_AddItemToArray(readonly_props, cJSON_CreateString(property_name));
            } else if (strcmp(property_name, "Password") == 0 ||
                       strcmp(property_name, "RoleId") == 0 ||
                       strcmp(property_name, "Enabled") == 0 ||
                       strcmp(property_name, "Locked") == 0) {
                // Writable property (but we'll still treat as read-only for this implementation)
                all_readonly = false;
                break;
            } else {
                // Unknown/unsupported property
                cJSON_AddItemToArray(readonly_props, cJSON_CreateString(property_name));
            }
        }
    }
    
    // If request contains only OData annotations, return NoOperation message
    if (only_odata_annotations && cJSON_GetArraySize(odata_props) > 0) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        
        cJSON *error_response = cJSON_CreateObject();
        cJSON *error = cJSON_CreateObject();
        cJSON *extended_info = cJSON_CreateArray();
        
        cJSON *message = cJSON_CreateObject();
        cJSON_AddStringToObject(message, "@odata.type", "#Message.v1_1_1.Message");
        cJSON_AddStringToObject(message, "MessageId", "Base.1.15.0.NoOperation");
        cJSON_AddStringToObject(message, "Message", "The request body submitted contain only OData annotations and no operation was performed.");
        cJSON *args = cJSON_CreateArray();
        cJSON_AddItemToObject(message, "MessageArgs", args);
        cJSON_AddStringToObject(message, "Severity", "Warning");
        cJSON_AddStringToObject(message, "Resolution", "Remove the OData annotations or add an updatable property to the request body and resubmit the request.");
        cJSON_AddItemToArray(extended_info, message);
        
        cJSON_AddStringToObject(error, "code", "Base.1.15.0.NoOperation");
        cJSON_AddStringToObject(error, "message", "The request body submitted contain only OData annotations and no operation was performed.");
        cJSON_AddItemToObject(error, "@Message.ExtendedInfo", extended_info);
        cJSON_AddItemToObject(error_response, "error", error);
        
        char *error_json = cJSON_Print(error_response);
        if (error_json) {
            strncpy(response->body, error_json, sizeof(response->body) - 1);
            response->body[sizeof(response->body) - 1] = '\0';
            free(error_json);
        } else {
            strcpy(response->body, "{\"error\":\"The request body submitted contain only OData annotations and no operation was performed.\"}");
        }
        
        cJSON_Delete(error_response);
        cJSON_Delete(readonly_props);
        cJSON_Delete(odata_props);
        cJSON_Delete(json);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
    
    if (all_readonly && cJSON_GetArraySize(readonly_props) > 0) {
        // Return 400 Bad Request with error details
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        
        cJSON *error_response = cJSON_CreateObject();
        cJSON *error = cJSON_CreateObject();
        cJSON *extended_info = cJSON_CreateArray();
        
        // Add error for each read-only property
        for (int i = 0; i < cJSON_GetArraySize(readonly_props); i++) {
            cJSON *prop = cJSON_GetArrayItem(readonly_props, i);
            if (prop && cJSON_IsString(prop)) {
                cJSON *message = cJSON_CreateObject();
                cJSON_AddStringToObject(message, "@odata.type", "#Message.v1_1_1.Message");
                cJSON_AddStringToObject(message, "MessageId", "Base.1.15.0.PropertyNotWritable");
                cJSON_AddStringToObject(message, "Message", "The property is a read-only property and cannot be assigned a value.");
                cJSON *args = cJSON_CreateArray();
                cJSON_AddItemToArray(args, cJSON_CreateString(prop->valuestring));
                cJSON_AddItemToObject(message, "MessageArgs", args);
                cJSON_AddStringToObject(message, "Severity", "Warning");
                cJSON_AddStringToObject(message, "Resolution", "Remove the property from the request body and resubmit the request if the operation failed.");
                cJSON_AddItemToArray(extended_info, message);
            }
        }
        
        cJSON_AddStringToObject(error, "code", "Base.1.15.0.PropertyNotWritable");
        cJSON_AddStringToObject(error, "message", "The request included read-only properties that cannot be updated.");
        cJSON_AddItemToObject(error, "@Message.ExtendedInfo", extended_info);
        cJSON_AddItemToObject(error_response, "error", error);
        
        char *error_json = cJSON_Print(error_response);
        if (error_json) {
            strncpy(response->body, error_json, sizeof(response->body) - 1);
            response->body[sizeof(response->body) - 1] = '\0';
            free(error_json);
        } else {
            strcpy(response->body, "{\"error\":\"The request included read-only properties that cannot be updated.\"}");
        }
        
        cJSON_Delete(error_response);
        cJSON_Delete(readonly_props);
        cJSON_Delete(odata_props);
        cJSON_Delete(json);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
    
    // If we reach here, there are some writable properties in the request
    // Now enforce ETag precondition if provided
    if (if_match_header) {
        char etag_source[256];
        snprintf(etag_source, sizeof(etag_source), "%s-%s-%d", username, role, id);
        unsigned int hash = 0;
        for (const char *p = etag_source; *p; p++) {
            hash = hash * 31 + (unsigned char)*p;
        }
        char current_etag[32];
        snprintf(current_etag, sizeof(current_etag), "\"%08x\"", hash);

        if (strcmp(if_match_header, current_etag) != 0 && strcmp(if_match_header, "*") != 0) {
            response->status_code = HTTP_PRECONDITION_FAILED;
            strcpy(response->content_type, CONTENT_TYPE_JSON);
            strcpy(response->body,
                "{"
                  "\"error\":{"
                    "\"code\":\"Base.1.15.0.PreconditionFailed\","
                    "\"message\":\"The ETag value provided did not match the current ETag of the resource.\""
                  "}"
                "}");
            response->content_length = strlen(response->body);
            return SUCCESS;
        }
    }
    // Extract writable fields and attempt to update via database
    char new_password[128] = {0};
    char new_role[64] = {0};
    int want_password = 0, want_role = 0, want_enabled = 0, want_locked = 0;
    bool new_enabled = false, new_locked = false;

    // Track additional failed properties (unsupported/invalid type)
    cJSON *failed_props = cJSON_CreateArray();

    cJSON_ArrayForEach(item, json) {
        const char *property_name = item->string;
        if (!property_name) continue;

        // Skip OData annotations here
        if (property_name[0] == '@') continue;

        if (strcmp(property_name, "Password") == 0) {
            if (cJSON_IsString(item) && item->valuestring) {
                strncpy(new_password, item->valuestring, sizeof(new_password) - 1);
                want_password = 1;
            } else {
                cJSON_AddItemToArray(failed_props, cJSON_CreateString("Password"));
            }
            continue;
        }
        if (strcmp(property_name, "RoleId") == 0) {
            if (cJSON_IsString(item) && item->valuestring) {
                strncpy(new_role, item->valuestring, sizeof(new_role) - 1);
                want_role = 1;
            } else {
                cJSON_AddItemToArray(failed_props, cJSON_CreateString("RoleId"));
            }
            continue;
        }
        if (strcmp(property_name, "Enabled") == 0) {
            if (cJSON_IsBool(item)) {
                new_enabled = cJSON_IsTrue(item);
                want_enabled = 1;
            } else {
                cJSON_AddItemToArray(failed_props, cJSON_CreateString("Enabled"));
            }
            continue;
        }
        if (strcmp(property_name, "Locked") == 0) {
            if (cJSON_IsBool(item)) {
                new_locked = cJSON_IsTrue(item);
                want_locked = 1;
            } else {
                cJSON_AddItemToArray(failed_props, cJSON_CreateString("Locked"));
            }
            continue;
        }

        // Read-only properties that were not caught in the earlier classification
        if (strcmp(property_name, "Id") == 0 ||
            strcmp(property_name, "Name") == 0 ||
            strcmp(property_name, "Description") == 0 ||
            strcmp(property_name, "UserName") == 0 ||
            strcmp(property_name, "Links") == 0) {
            cJSON_AddItemToArray(readonly_props, cJSON_CreateString(property_name));
            continue;
        }

        // Any other unknown property
        cJSON_AddItemToArray(readonly_props, cJSON_CreateString(property_name));
    }

    int updated_pw = 0, updated_role = 0, updated_enabled = 0, updated_locked = 0;
    const char *pw_arg = want_password ? new_password : NULL;
    const char *role_arg = want_role ? new_role : NULL;
    int upd_rc = account_update(id, pw_arg, role_arg, &updated_pw, &updated_role);

    // Update enabled and locked fields if requested
    if (want_enabled) {
        if (account_set_enabled(id, new_enabled) == SUCCESS) {
            updated_enabled = 1;
        } else {
            cJSON_AddItemToArray(failed_props, cJSON_CreateString("Enabled"));
        }
    }
    if (want_locked) {
        if (account_set_locked(id, new_locked) == SUCCESS) {
            updated_locked = 1;
        } else {
            cJSON_AddItemToArray(failed_props, cJSON_CreateString("Locked"));
        }
    }

    // Build list of properties that could not be updated (requested but not changed)
    if (want_password && !updated_pw) {
        cJSON_AddItemToArray(failed_props, cJSON_CreateString("Password"));
    }
    if (want_role && !updated_role) {
        cJSON_AddItemToArray(failed_props, cJSON_CreateString("RoleId"));
    }

    // If database update failed entirely
    if (upd_rc != SUCCESS && (want_password || want_role)) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":\"Failed to update account\"}");
        cJSON_Delete(readonly_props);
        cJSON_Delete(odata_props);
        cJSON_Delete(failed_props);
        cJSON_Delete(json);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // If at least one property updated successfully, return 200 OK with resource and extended info for failures
    bool any_updated = (updated_pw || updated_role || updated_enabled || updated_locked);
    if (any_updated) {
        // Refresh role value used in representation if updated
        if (updated_role && new_role[0] != '\0') {
            strncpy(role, new_role, sizeof(role) - 1);
            role[sizeof(role) - 1] = '\0';
        }

        // Get current account state for ETag and response
        account_info_t current_account;
        bool current_enabled = true, current_locked = false;
        if (account_get_by_id(id, &current_account) == SUCCESS) {
            current_enabled = current_account.enabled;
            current_locked = current_account.locked;
        }

        // Recompute ETag (based on username, role, id, enabled, locked)
        char etag_source2[256];
        snprintf(etag_source2, sizeof(etag_source2), "%s-%s-%d-%d-%d", username, role, id, 
                 current_enabled ? 1 : 0, current_locked ? 1 : 0);
        unsigned int hash2 = 0;
        for (const char *p = etag_source2; *p; p++) {
            hash2 = hash2 * 31 + (unsigned char)*p;
        }
        char etag2[32];
        snprintf(etag2, sizeof(etag2), "\"%08x\"", hash2);

        response->status_code = HTTP_OK;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        if (response->header_count < MAX_HEADERS) {
            strcpy(response->headers[response->header_count][0], "ETag");
            strcpy(response->headers[response->header_count][1], etag2);
            response->header_count++;
        }

        cJSON *response_json = cJSON_CreateObject();
        cJSON_AddStringToObject(response_json, "@odata.type", "#ManagerAccount.v1_12_0.ManagerAccount");
        char odata_id2[128];
        snprintf(odata_id2, sizeof(odata_id2), "/redfish/v1/AccountService/Accounts/%d", id);
        cJSON_AddStringToObject(response_json, "@odata.id", odata_id2);
        cJSON_AddStringToObject(response_json, "Id", account_id);
        cJSON_AddStringToObject(response_json, "Name", "User Account");
        cJSON_AddStringToObject(response_json, "Description", "User Account");
        cJSON_AddStringToObject(response_json, "UserName", username);
        cJSON_AddStringToObject(response_json, "RoleId", role);
        cJSON_AddBoolToObject(response_json, "Enabled", current_enabled);
        cJSON_AddBoolToObject(response_json, "Locked", current_locked);
        
        // Add AccountTypes array with default "Redfish" value
        cJSON *account_types = cJSON_CreateArray();
        cJSON_AddItemToArray(account_types, cJSON_CreateString("Redfish"));
        cJSON_AddItemToObject(response_json, "AccountTypes", account_types);

        // Build extended info for read-only/unknown and failed requested properties
        if (cJSON_GetArraySize(readonly_props) > 0 || cJSON_GetArraySize(failed_props) > 0) {
            cJSON *extended_info = cJSON_CreateArray();
            for (int i = 0; i < cJSON_GetArraySize(readonly_props); i++) {
                cJSON *prop = cJSON_GetArrayItem(readonly_props, i);
                if (prop && cJSON_IsString(prop)) {
                    cJSON *message = cJSON_CreateObject();
                    cJSON_AddStringToObject(message, "@odata.type", "#Message.v1_1_1.Message");
                    cJSON_AddStringToObject(message, "MessageId", "Base.1.15.0.PropertyNotWritable");
                    cJSON_AddStringToObject(message, "Message", "The property is a read-only property and cannot be assigned a value.");
                    cJSON *args = cJSON_CreateArray();
                    cJSON_AddItemToArray(args, cJSON_CreateString(prop->valuestring));
                    cJSON_AddItemToObject(message, "MessageArgs", args);
                    cJSON_AddStringToObject(message, "Severity", "Warning");
                    cJSON_AddStringToObject(message, "Resolution", "Remove the property from the request body and resubmit the request if the operation failed.");
                    cJSON_AddItemToArray(extended_info, message);

                    // Also add per-property @Message.ExtendedInfo annotation
                    char key[160];
                    snprintf(key, sizeof(key), "%s@Message.ExtendedInfo", prop->valuestring);
                    cJSON *per_prop_arr = cJSON_CreateArray();
                    cJSON *msg_copy = cJSON_CreateObject();
                    cJSON_AddStringToObject(msg_copy, "@odata.type", "#Message.v1_1_1.Message");
                    cJSON_AddStringToObject(msg_copy, "MessageId", "Base.1.15.0.PropertyNotWritable");
                    cJSON_AddStringToObject(msg_copy, "Message", "The property is a read-only property and cannot be assigned a value.");
                    cJSON *args2 = cJSON_CreateArray();
                    cJSON_AddItemToArray(args2, cJSON_CreateString(prop->valuestring));
                    cJSON_AddItemToObject(msg_copy, "MessageArgs", args2);
                    cJSON_AddStringToObject(msg_copy, "Severity", "Warning");
                    cJSON_AddStringToObject(msg_copy, "Resolution", "Remove the property from the request body and resubmit the request if the operation failed.");
                    cJSON_AddItemToArray(per_prop_arr, msg_copy);
                    cJSON_AddItemToObject(response_json, key, per_prop_arr);
                }
            }
            for (int i = 0; i < cJSON_GetArraySize(failed_props); i++) {
                cJSON *prop = cJSON_GetArrayItem(failed_props, i);
                if (prop && cJSON_IsString(prop)) {
                    cJSON *message = cJSON_CreateObject();
                    cJSON_AddStringToObject(message, "@odata.type", "#Message.v1_1_1.Message");
                    cJSON_AddStringToObject(message, "MessageId", "Base.1.15.0.PropertyNotWritable");
                    cJSON_AddStringToObject(message, "Message", "The property could not be updated.");
                    cJSON *args = cJSON_CreateArray();
                    cJSON_AddItemToArray(args, cJSON_CreateString(prop->valuestring));
                    cJSON_AddItemToObject(message, "MessageArgs", args);
                    cJSON_AddStringToObject(message, "Severity", "Warning");
                    cJSON_AddStringToObject(message, "Resolution", "Remove the property from the request body or ensure it is supported, then resubmit the request.");
                    cJSON_AddItemToArray(extended_info, message);

                    // Also add per-property @Message.ExtendedInfo annotation
                    char key2[160];
                    snprintf(key2, sizeof(key2), "%s@Message.ExtendedInfo", prop->valuestring);
                    cJSON *per_prop_arr2 = cJSON_CreateArray();
                    cJSON *msg_copy2 = cJSON_CreateObject();
                    cJSON_AddStringToObject(msg_copy2, "@odata.type", "#Message.v1_1_1.Message");
                    cJSON_AddStringToObject(msg_copy2, "MessageId", "Base.1.15.0.PropertyNotWritable");
                    cJSON_AddStringToObject(msg_copy2, "Message", "The property could not be updated.");
                    cJSON *args3 = cJSON_CreateArray();
                    cJSON_AddItemToArray(args3, cJSON_CreateString(prop->valuestring));
                    cJSON_AddItemToObject(msg_copy2, "MessageArgs", args3);
                    cJSON_AddStringToObject(msg_copy2, "Severity", "Warning");
                    cJSON_AddStringToObject(msg_copy2, "Resolution", "Remove the property from the request body or ensure it is supported, then resubmit the request.");
                    cJSON_AddItemToArray(per_prop_arr2, msg_copy2);
                    cJSON_AddItemToObject(response_json, key2, per_prop_arr2);
                }
            }
            cJSON_AddItemToObject(response_json, "@Message.ExtendedInfo", extended_info);
        }

        char *response_str2 = cJSON_Print(response_json);
        if (response_str2) {
            strncpy(response->body, response_str2, sizeof(response->body) - 1);
            response->body[sizeof(response->body) - 1] = '\0';
            free(response_str2);
        } else {
            strcpy(response->body, "{\"error\":\"Failed to generate response\"}");
        }

        cJSON_Delete(response_json);
        cJSON_Delete(readonly_props);
        cJSON_Delete(odata_props);
        cJSON_Delete(failed_props);
        cJSON_Delete(json);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // No property actually updated (e.g., only unsupported writables provided). Return 200 with extended info as well.
    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);

    // ETag stays the same as before since no change; recompute for consistency
    char etag_source3[256];
    snprintf(etag_source3, sizeof(etag_source3), "%s-%s-%d", username, role, id);
    unsigned int hash3 = 0;
    for (const char *p = etag_source3; *p; p++) {
        hash3 = hash3 * 31 + (unsigned char)*p;
    }
    char etag3[32];
    snprintf(etag3, sizeof(etag3), "\"%08x\"", hash3);
    if (response->header_count < MAX_HEADERS) {
        strcpy(response->headers[response->header_count][0], "ETag");
        strcpy(response->headers[response->header_count][1], etag3);
        response->header_count++;
    }

    cJSON *response_json3 = cJSON_CreateObject();
    cJSON_AddStringToObject(response_json3, "@odata.type", "#ManagerAccount.v1_12_0.ManagerAccount");
    char odata_id3[128];
    snprintf(odata_id3, sizeof(odata_id3), "/redfish/v1/AccountService/Accounts/%d", id);
    cJSON_AddStringToObject(response_json3, "@odata.id", odata_id3);
    cJSON_AddStringToObject(response_json3, "Id", account_id);
    cJSON_AddStringToObject(response_json3, "Name", "User Account");
    cJSON_AddStringToObject(response_json3, "Description", "User Account");
    cJSON_AddStringToObject(response_json3, "UserName", username);
    cJSON_AddStringToObject(response_json3, "RoleId", role);
    cJSON_AddBoolToObject(response_json3, "Enabled", true);
    cJSON_AddBoolToObject(response_json3, "Locked", false);
    
    // Add AccountTypes array with default "Redfish" value
    cJSON *account_types3 = cJSON_CreateArray();
    cJSON_AddItemToArray(account_types3, cJSON_CreateString("Redfish"));
    cJSON_AddItemToObject(response_json3, "AccountTypes", account_types3);

    // Build extended info from read-only and failed props arrays
    if (cJSON_GetArraySize(readonly_props) > 0 || cJSON_GetArraySize(failed_props) > 0) {
        cJSON *extended_info3 = cJSON_CreateArray();
        for (int i = 0; i < cJSON_GetArraySize(readonly_props); i++) {
            cJSON *prop = cJSON_GetArrayItem(readonly_props, i);
            if (prop && cJSON_IsString(prop)) {
                cJSON *message = cJSON_CreateObject();
                cJSON_AddStringToObject(message, "@odata.type", "#Message.v1_1_1.Message");
                cJSON_AddStringToObject(message, "MessageId", "Base.1.15.0.PropertyNotWritable");
                cJSON_AddStringToObject(message, "Message", "The property is a read-only property and cannot be assigned a value.");
                cJSON *args = cJSON_CreateArray();
                cJSON_AddItemToArray(args, cJSON_CreateString(prop->valuestring));
                cJSON_AddItemToObject(message, "MessageArgs", args);
                cJSON_AddStringToObject(message, "Severity", "Warning");
                cJSON_AddStringToObject(message, "Resolution", "Remove the property from the request body and resubmit the request if the operation failed.");
                cJSON_AddItemToArray(extended_info3, message);

                // Per-property annotation
                char key3[160];
                snprintf(key3, sizeof(key3), "%s@Message.ExtendedInfo", prop->valuestring);
                cJSON *per_prop_arr3 = cJSON_CreateArray();
                cJSON *msg_copy3 = cJSON_CreateObject();
                cJSON_AddStringToObject(msg_copy3, "@odata.type", "#Message.v1_1_1.Message");
                cJSON_AddStringToObject(msg_copy3, "MessageId", "Base.1.15.0.PropertyNotWritable");
                cJSON_AddStringToObject(msg_copy3, "Message", "The property is a read-only property and cannot be assigned a value.");
                cJSON *args4 = cJSON_CreateArray();
                cJSON_AddItemToArray(args4, cJSON_CreateString(prop->valuestring));
                cJSON_AddItemToObject(msg_copy3, "MessageArgs", args4);
                cJSON_AddStringToObject(msg_copy3, "Severity", "Warning");
                cJSON_AddStringToObject(msg_copy3, "Resolution", "Remove the property from the request body and resubmit the request if the operation failed.");
                cJSON_AddItemToArray(per_prop_arr3, msg_copy3);
                cJSON_AddItemToObject(response_json3, key3, per_prop_arr3);
            }
        }
        for (int i = 0; i < cJSON_GetArraySize(failed_props); i++) {
            cJSON *prop = cJSON_GetArrayItem(failed_props, i);
            if (prop && cJSON_IsString(prop)) {
                cJSON *message = cJSON_CreateObject();
                cJSON_AddStringToObject(message, "@odata.type", "#Message.v1_1_1.Message");
                cJSON_AddStringToObject(message, "MessageId", "Base.1.15.0.PropertyNotWritable");
                cJSON_AddStringToObject(message, "Message", "The property could not be updated.");
                cJSON *args = cJSON_CreateArray();
                cJSON_AddItemToArray(args, cJSON_CreateString(prop->valuestring));
                cJSON_AddItemToObject(message, "MessageArgs", args);
                cJSON_AddStringToObject(message, "Severity", "Warning");
                cJSON_AddStringToObject(message, "Resolution", "Remove the property from the request body or ensure it is supported, then resubmit the request.");
                cJSON_AddItemToArray(extended_info3, message);

                // Per-property annotation
                char key4[160];
                snprintf(key4, sizeof(key4), "%s@Message.ExtendedInfo", prop->valuestring);
                cJSON *per_prop_arr4 = cJSON_CreateArray();
                cJSON *msg_copy4 = cJSON_CreateObject();
                cJSON_AddStringToObject(msg_copy4, "@odata.type", "#Message.v1_1_1.Message");
                cJSON_AddStringToObject(msg_copy4, "MessageId", "Base.1.15.0.PropertyNotWritable");
                cJSON_AddStringToObject(msg_copy4, "Message", "The property could not be updated.");
                cJSON *args5 = cJSON_CreateArray();
                cJSON_AddItemToArray(args5, cJSON_CreateString(prop->valuestring));
                cJSON_AddItemToObject(msg_copy4, "MessageArgs", args5);
                cJSON_AddStringToObject(msg_copy4, "Severity", "Warning");
                cJSON_AddStringToObject(msg_copy4, "Resolution", "Remove the property from the request body or ensure it is supported, then resubmit the request.");
                cJSON_AddItemToArray(per_prop_arr4, msg_copy4);
                cJSON_AddItemToObject(response_json3, key4, per_prop_arr4);
            }
        }
        cJSON_AddItemToObject(response_json3, "@Message.ExtendedInfo", extended_info3);
    }

    char *response_str3 = cJSON_Print(response_json3);
    if (response_str3) {
        strncpy(response->body, response_str3, sizeof(response->body) - 1);
        response->body[sizeof(response->body) - 1] = '\0';
        free(response_str3);
    } else {
        strcpy(response->body, "{\"error\":\"Failed to generate response\"}");
    }

    cJSON_Delete(response_json3);
    cJSON_Delete(readonly_props);
    cJSON_Delete(odata_props);
    cJSON_Delete(failed_props);
    cJSON_Delete(json);
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_certificate_service(http_response_t *response)
{
    if (!response) return ERROR_INVALID_PARAM;

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);

    snprintf(response->body, sizeof(response->body),
        "{\n"
        "  \"@odata.type\": \"#CertificateService.v1_1_0.CertificateService\",\n"
        "  \"@odata.id\": \"/redfish/v1/CertificateService\",\n"
        "  \"Id\": \"CertificateService\",\n"
        "  \"Name\": \"Certificate Service\",\n"
        "  \"Description\": \"Service for managing certificates\",\n"
        "  \"Actions\": {\n"
        "    \"#CertificateService.GenerateCSR\": {\n"
        "      \"target\": \"/redfish/v1/CertificateService/Actions/CertificateService.GenerateCSR\"\n"
        "    },\n"
        "    \"#CertificateService.ReplaceCertificate\": {\n"
        "      \"target\": \"/redfish/v1/CertificateService/Actions/CertificateService.ReplaceCertificate\"\n"
        "    }\n"
        "  }\n"
        "}"
    );

    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_certificate_service_generate_csr(const http_request_t *request, http_response_t *response) {
    if (!request || !response) return ERROR_INVALID_PARAM;

    certificate_csr_request_t req;
    memset(&req, 0, sizeof(req));
    req.key_bit_length = 0;

    // Parse the request body to get CSR parameters
    cJSON *json = cJSON_Parse(request->body);
    if (!json) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.MalformedJSON\",\"message\":\"The request body contains malformed JSON.\"}}");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Extract common CSR parameters
    cJSON *common_name = cJSON_GetObjectItem(json, "CommonName");
    cJSON *country = cJSON_GetObjectItem(json, "Country");
    cJSON *state = cJSON_GetObjectItem(json, "State");
    cJSON *city = cJSON_GetObjectItem(json, "City");
    cJSON *organization = cJSON_GetObjectItem(json, "Organization");
    cJSON *organizational_unit = cJSON_GetObjectItem(json, "OrganizationalUnit");

    // Additional CSR parameters
    do {
        cJSON *certificate_collection = cJSON_GetObjectItem(json, "CertificateCollection");
        if (certificate_collection && cJSON_IsObject(certificate_collection)) {
            cJSON *odata_id = cJSON_GetObjectItem(certificate_collection, "@odata.id");
            if (odata_id && cJSON_IsString(odata_id)) {
                strncpy(req.certificate_collection_odata_id, cJSON_GetStringValue(odata_id), sizeof(req.certificate_collection_odata_id) - 1);
            }
        }
    } while (0);

    cJSON *alternative_names = cJSON_GetObjectItem(json, "AlternativeNames");
    cJSON *key_usage = cJSON_GetObjectItem(json, "KeyUsage");
    cJSON *key_pair_algorithm = cJSON_GetObjectItem(json, "KeyPairAlgorithm");
    cJSON *key_bit_length = cJSON_GetObjectItem(json, "KeyBitLength");
    cJSON *hash_algorithm = cJSON_GetObjectItem(json, "HashAlgorithm");

    // Validate required parameters
    if (!common_name || !cJSON_IsString(common_name)) {
        cJSON_Delete(json);
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.PropertyMissing\",\"message\":\"The property CommonName is required in the request body.\"}}");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    const char *cn = cJSON_GetStringValue(common_name);
    const char *c = country && cJSON_IsString(country) ? cJSON_GetStringValue(country) : "TW";
    const char *s = state && cJSON_IsString(state) ? cJSON_GetStringValue(state) : "Taipei";
    const char *l = city && cJSON_IsString(city) ? cJSON_GetStringValue(city) : "Taipei";
    const char *o = organization && cJSON_IsString(organization) ? cJSON_GetStringValue(organization) : MANAGER_ID_KENMEC;
    const char *ou = organizational_unit && cJSON_IsString(organizational_unit) ? cJSON_GetStringValue(organizational_unit) : "IT Department";

    strncpy(req.common_name, cn, sizeof(req.common_name) - 1);
    strncpy(req.country, c, sizeof(req.country) - 1);
    strncpy(req.state, s, sizeof(req.state) - 1);
    strncpy(req.city, l, sizeof(req.city) - 1);
    strncpy(req.organization, o, sizeof(req.organization) - 1);
    strncpy(req.organizational_unit, ou, sizeof(req.organizational_unit) - 1);

    if (key_pair_algorithm && cJSON_IsString(key_pair_algorithm)) {
        strncpy(req.key_pair_algorithm, cJSON_GetStringValue(key_pair_algorithm), sizeof(req.key_pair_algorithm) - 1);
    } else {
        strncpy(req.key_pair_algorithm, "ECDSA", sizeof(req.key_pair_algorithm) - 1);
    }
    if (key_bit_length && cJSON_IsNumber(key_bit_length)) {
        req.key_bit_length = key_bit_length->valueint;
    } else {
        req.key_bit_length = 256;
    }
    if (hash_algorithm && cJSON_IsString(hash_algorithm)) {
        strncpy(req.hash_algorithm, cJSON_GetStringValue(hash_algorithm), sizeof(req.hash_algorithm) - 1);
    } else {
        strncpy(req.hash_algorithm, "SHA256", sizeof(req.hash_algorithm) - 1);
    }

    req.alternative_name_count = 0;
    if (alternative_names && cJSON_IsArray(alternative_names)) {
        int n = cJSON_GetArraySize(alternative_names);
        if (n > 10) n = 10;
        for (int i = 0; i < n; i++) {
            cJSON *item = cJSON_GetArrayItem(alternative_names, i);
            if (item && cJSON_IsString(item)) {
                strncpy(req.alternative_names[req.alternative_name_count], cJSON_GetStringValue(item), sizeof(req.alternative_names[0]) - 1);
                req.alternative_name_count++;
            }
        }
    }

    req.key_usage_count = 0;
    if (key_usage && cJSON_IsArray(key_usage)) {
        int n = cJSON_GetArraySize(key_usage);
        if (n > 10) n = 10;
        for (int i = 0; i < n; i++) {
            cJSON *item = cJSON_GetArrayItem(key_usage, i);
            if (item && cJSON_IsString(item)) {
                strncpy(req.key_usage[req.key_usage_count], cJSON_GetStringValue(item), sizeof(req.key_usage[0]) - 1);
                req.key_usage_count++;
            }
        }
    }

    char alt_buf[512];
    alt_buf[0] = '\0';
    for (int i = 0; i < req.alternative_name_count; i++) {
        if (i > 0) strncat(alt_buf, ", ", sizeof(alt_buf) - strlen(alt_buf) - 1);
        strncat(alt_buf, req.alternative_names[i], sizeof(alt_buf) - strlen(alt_buf) - 1);
    }

    char ku_buf[512];
    ku_buf[0] = '\0';
    for (int i = 0; i < req.key_usage_count; i++) {
        if (i > 0) strncat(ku_buf, ", ", sizeof(ku_buf) - strlen(ku_buf) - 1);
        strncat(ku_buf, req.key_usage[i], sizeof(ku_buf) - strlen(ku_buf) - 1);
    }

    printf("\nCertificateService.GenerateCSR action called with CN: %s, C: %s, S: %s, L: %s, O: %s, OU: %s\n",
           req.common_name, req.country, req.state, req.city, req.organization, req.organizational_unit);
    printf("CSR params: CertColl=%s, AltNames=[%s], KeyUsage=[%s], KeyPairAlgorithm=%s, KeyBitLength=%d, HashAlgorithm=%s\n\n",
           req.certificate_collection_odata_id[0] ? req.certificate_collection_odata_id : "(null)",
           alt_buf,
           ku_buf,
           req.key_pair_algorithm,
           req.key_bit_length,
           req.hash_algorithm);

    char csr_str[4096];
    if (redfish_generate_csr(&req, csr_str, sizeof(csr_str)) != 0) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body,
            "{"
                "\"error\":{"
                    "\"code\":\"Base.1.15.0.InternalError\","
                    "\"message\":\"The service failed to generate the certificate signing request (CSR).\""
                "}"
            "}"
        );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Escape PEM for JSON (convert newlines to \n and escape backslashes and quotes)
    char csr_json[8192];
    redfish_escape_pem_for_json(csr_str, csr_json, sizeof(csr_json));

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    snprintf(response->body, sizeof(response->body),
        "{\n"
        "  \"@odata.type\": \"#CertificateService.v1_1_0.GenerateCSRResponse\",\n"
        "  \"@Redfish.SettingsApplyTime\": { \"ApplyTime\": \"OnReset\" },\n"
        "  \"CSRString\": \"%s\",\n"
        "  \"CertificateCollection\": {\n"
        "    \"@odata.id\": \"/redfish/v1/CertificateService/Certificates\"\n"
        "  }\n"
        "}\n",
        csr_json
    );

    response->content_length = strlen(response->body);

    cJSON_Delete(json);

    return SUCCESS;
}

int handle_chassis_collection(http_response_t *response) {
    char *json = generate_chassis_collection_json();
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to generate chassis collection\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int handle_chassis(const char *chassis_id, http_response_t *response) {
    if (!chassis_id) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->body, "{\"error\":\"Chassis ID required\"}");
        return ERROR_INVALID_PARAM;
    }

    char *json = generate_chassis_json(chassis_id);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to generate chassis data\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int handle_systems_collection(http_response_t *response) {
    char *json = generate_systems_collection_json();
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to generate systems collection\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int handle_system(const char *system_id, http_response_t *response) {
    if (!system_id) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->body, "{\"error\":\"System ID required\"}");
        return ERROR_INVALID_PARAM;
    }

    char *json = generate_system_json(system_id);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to generate system data\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int get_managers_ethernet_interface_eth0(const char *resource_id, http_response_t *response) {
    char *json = open_json_file(resource_id, MANAGER_ETHERNET_INTERFACE_ETH0);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to generate managers collection\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int get_managers_ethernet_interface(const char *resource_id, http_response_t *response) {
    char *json = open_json_file(resource_id, MANAGER_ETHERNET_INTERFACE);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to generate managers collection\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int get_managers_kenmec_resource(const char *resource_id, http_response_t *response) {
    char *json = generate_manager_json(resource_id);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to generate manager data\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int get_managers_collection(const char *resource_id, http_response_t *response) {
    char *json = open_json_file(resource_id, MANAGER_COLLECTION);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to generate managers collection\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int handle_manager(const char *manager_id, http_response_t *response) {
    if (!manager_id) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->body, "{\"error\":\"Manager ID required\"}");
        return ERROR_INVALID_PARAM;
    }

    char *json = generate_manager_json(manager_id);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to generate manager data\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int handle_token_invalid(http_response_t *response)
{
    response->status_code = HTTP_UNAUTHORIZED;
    strcpy(response->content_type, "application/json");
    snprintf(response->body, sizeof(response->body),
             "{ \"error\": \"Authentication token required or invalid\" }");
    response->content_length = strlen(response->body);

    // Add WWW-Authenticate header for 401 responses
    if (response->header_count < MAX_HEADERS) {
        strcpy(response->headers[response->header_count][0], "WWW-Authenticate");
        strcpy(response->headers[response->header_count][1], "Basic realm=\"Redfish\", Bearer");
        response->header_count++;
    }

    return SUCCESS;
}

char* generate_service_root_json(void) {
    time_t now = time(NULL);
    char time_str[64];
    char uuid[37]; // UUID format: 36 chars + null terminator
    
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    // Get UUID from redfish_get_uuid function
    if (redfish_get_uuid(uuid, sizeof(uuid)) != 0) {
        // If UUID retrieval fails, use a default UUID
        strcpy(uuid, "12345678-1234-1234-1234-123456789012");
    }

    char *json = malloc(2048);
    if (!json) return NULL;

    snprintf(json, 2048,
        "{"
        "\"@odata.type\":\"#ServiceRoot.v1_19_0.ServiceRoot\","
        "\"@odata.id\":\"/redfish/v1/\","
        "\"Id\":\"RootService\","
        "\"Name\":\"Root Service\","
        "\"RedfishVersion\":\"%s\","
        "\"UUID\":\"%s\","
        "\"SessionService\":{\"@odata.id\":\"/redfish/v1/SessionService\"},"
        "\"AccountService\":{\"@odata.id\":\"/redfish/v1/AccountService\"},"
        "\"CertificateService\":{\"@odata.id\":\"/redfish/v1/CertificateService\"},"
        "\"UpdateService\":{\"@odata.id\":\"/redfish/v1/UpdateService\"},"
        "\"Managers\":{\"@odata.id\":\"/redfish/v1/Managers\"},"
        // "\"Chassis\":{\"@odata.id\":\"/redfish/v1/Chassis\"}," // TODO: Add Chassis back in if needed.
        "\"ThermalEquipment\":{\"@odata.id\":\"/redfish/v1/ThermalEquipment\"},"
        "\"Links\":{\"Sessions\":{\"@odata.id\":\"/redfish/v1/SessionService/Sessions\"}}"
        "}",
        REDFISH_VERSION, uuid);

    return json;
}

char* generate_chassis_collection_json(void) {
    char *json = malloc(1024);
    if (!json) return NULL;

    snprintf(json, 1024,
        "{"
        "\"@odata.type\":\"#ChassisCollection.ChassisCollection\","
        "\"@odata.id\":\"/redfish/v1/Chassis\","
        "\"Name\":\"Chassis Collection\","
        "\"Description\":\"Collection of Chassis\","
        "\"Members@odata.count\":1,"
        "\"Members\":["
        "{"
        "\"@odata.id\":\"/redfish/v1/Chassis/%s\""
        "}"
        "],"
        "\"@odata.count\":1"
        "}",
        CHASSIS_ID);

    return json;
}

char* generate_chassis_json(const char *chassis_id) {
    char *json = malloc(2048);
    if (!json) return NULL;

    snprintf(json, 2048,
        "{"
        "\"@odata.type\":\"#Chassis.v1_20_0.Chassis\","
        "\"@odata.id\":\"/redfish/v1/Chassis/%s\","
        "\"Id\":\"%s\","
        "\"Name\":\"Main System Chassis\","
        "\"Description\":\"Main system chassis\","
        "\"ChassisType\":\"RackMount\","
        "\"Model\":\"RedfishDemo Chassis\","
        "\"Manufacturer\":\"RedfishDemo Inc.\","
        "\"SerialNumber\":\"CHASSIS-%s\","
        "\"PartNumber\":\"CHASSIS-001\","
        "\"SKU\":\"CHASSIS-SKU-001\","
        "\"Status\":{"
        "\"State\":\"Enabled\","
        "\"Health\":\"OK\""
        "},"
        "\"PowerState\":\"On\","
        "\"Thermal\":{\"@odata.id\":\"/redfish/v1/Chassis/%s/Thermal\"},"
        "\"Power\":{\"@odata.id\":\"/redfish/v1/Chassis/%s/Power\"},"
        "\"Links\":{"
        "\"ComputerSystems\":[{\"@odata.id\":\"/redfish/v1/Systems/%s\"}],"
        "\"ManagedBy\":[{\"@odata.id\":\"/redfish/v1/Managers/%s\"}]"
        "},"
        "\"Oem\":{}"
        "}",
        chassis_id, chassis_id, chassis_id, chassis_id, chassis_id, SYSTEM_ID, MANAGER_ID);

    return json;
}

char* generate_systems_collection_json(void) {
    char *json = malloc(1024);
    if (!json) return NULL;

    snprintf(json, 1024,
        "{"
        "\"@odata.type\":\"#ComputerSystemCollection.ComputerSystemCollection\","
        "\"@odata.id\":\"/redfish/v1/Systems\","
        "\"Name\":\"Computer System Collection\","
        "\"Description\":\"Collection of Computer Systems\","
        "\"Members@odata.count\":1,"
        "\"Members\":["
        "{"
        "\"@odata.id\":\"/redfish/v1/Systems/%s\""
        "}"
        "],"
        "\"@odata.count\":1"
        "}",
        SYSTEM_ID);

    return json;
}

char* generate_system_json(const char *system_id) {
    char *json = malloc(3072);
    if (!json) return NULL;

    snprintf(json, 3072,
        "{"
        "\"@odata.type\":\"#ComputerSystem.v1_20_0.ComputerSystem\"," 
        "\"@odata.id\":\"/redfish/v1/Systems/%s\"," 
        "\"Id\":\"%s\"," 
        "\"Name\":\"Computer System\","
        "\"Description\":\"Main computer system\","
        "\"SystemType\":\"Physical\","
        "\"Manufacturer\":\"RedfishDemo Inc.\","
        "\"Model\":\"RedfishDemo System\","
        "\"SKU\":\"SYSTEM-SKU-001\","
        "\"SerialNumber\":\"SYSTEM-%s\","
        "\"PartNumber\":\"SYSTEM-001\","
        "\"AssetTag\":\"ASSET-TAG-001\","
        "\"Status\":{\"State\":\"Enabled\",\"Health\":\"OK\"},"
        "\"PowerState\":\"On\","
        "\"Boot\":{\"BootSourceOverrideEnabled\":\"Disabled\",\"BootSourceOverrideTarget\":\"None\",\"BootSourceOverrideMode\":\"UEFI\"},"
        "\"Links\":{\"Chassis\":[{\"@odata.id\":\"/redfish/v1/Chassis/%s\"}]},"
        "\"Oem\":{}"
        "}",
        system_id, system_id, system_id, CHASSIS_ID);

    return json;
}


char* open_resource_json(json_resource_type_t type) {
    const char *func_name = __func__;
    char catch_name[128];
    
    extract_func_middle(func_name, catch_name, sizeof(catch_name));
    char *json = open_json_file(catch_name, type);


    cJSON *root = cJSON_Parse(json);
    if (!root) {
        printf("JSON parse error\n");
        return NULL;
    }
    
    cJSON_Delete(root);
    return json;
}



char* generate_manager_json(const char *manager_id) {
    char *json = malloc(1024);
    if (!json) return NULL;

    char uuid[37]; // UUID format: 36 chars + null terminator
    char firmware_version[32]; // Firmware version string
    
    // Get UUID from database
    if (redfish_get_uuid(uuid, sizeof(uuid)) != 0) {
        // If UUID retrieval fails, use a default UUID
        strcpy(uuid, "12345678-1234-1234-1234-123456789012");
    }

    // Construct firmware version from config macros
    snprintf(firmware_version, sizeof(firmware_version), "%d.%d.%d", 
             CONFIG_APPLICATION_MAJOR_VERSION, 
             CONFIG_APPLICATION_MINOR_VERSION, 
             CONFIG_APPLICATION_PATCH_VERSION);

    snprintf(json, 1024,
        "{"
        "\"@odata.type\":\"#Manager.v1_20_0.Manager\","
        "\"@odata.id\":\"/redfish/v1/Managers/%s\","
        "\"Id\":\"%s\","
        "\"Name\":\"CDU Manager\","
        "\"Description\":\"Coolant Distribution Unit Manager\","
        "\"ManagerType\":\"ManagementController\","
        "\"UUID\":\"%s\","
        "\"FirmwareVersion\":\"%s\","
        "\"Status\":{\"State\":\"Enabled\",\"Health\":\"OK\"},"
        "\"SecurityPolicy\":{\"@odata.id\":\"/redfish/v1/Managers/%s/SecurityPolicy\"},"
        "\"EthernetInterfaces\":{\"@odata.id\":\"/redfish/v1/Managers/%s/EthernetInterfaces\"},"
        "\"NetworkProtocol\":{\"@odata.id\":\"/redfish/v1/Managers/%s/NetworkProtocol\"},"
        "\"Actions\":{\"#Manager.Reset\":{\"target\":\"/redfish/v1/Managers/%s/Actions/Manager.Reset\",\"ResetType@Redfish.AllowableValues\":[\"ForceRestart\"]}}"
        "}",
        manager_id, manager_id, uuid, firmware_version, manager_id, manager_id, manager_id, manager_id);

    return json;
}

int get_manager_security_policy(const char *manager_id, http_response_t *response) {
    if (!manager_id || !response) return ERROR_INVALID_PARAM;

    int verify = 0;
    security_policy_t policy;
    if (security_policy_get(manager_id, &policy) == 0) {
        verify = policy.verify_certificate ? 1 : 0;
    }

    if (g_securitypolicy_applytime_onreset) {
        snprintf(response->body, sizeof(response->body),
            "{\n"
            "  \"@odata.type\": \"#SecurityPolicy.v1_0_3.SecurityPolicy\",\n"
            "  \"@odata.id\": \"/redfish/v1/Managers/%s/SecurityPolicy\",\n"
            "  \"Id\": \"SecurityPolicy\",\n"
            "  \"Name\": \"Manager Security Policy\",\n"
            "  \"Description\": \"Security policy for this Manager\",\n"
            "  \"@Redfish.SettingsApplyTime\": { \"ApplyTime\": \"OnReset\" },\n"
            "  \"TLS\": {\n"
            "    \"Server\": {\n"
            "      \"Allowed\": {\n"
            "        \"Versions\": [\"1.2\", \"1.3\"]\n"
            "      },\n"
            "      \"VerifyCertificate\": %s,\n"
            "      \"TrustedCertificates\": {\n"
            "        \"@odata.id\": \"/redfish/v1/Managers/Kenmec/SecurityPolicy/TLS/Server/TrustedCertificates\"\n"
            "      }\n"
            "    }\n"
            "  }\n"
            "}",
            manager_id, verify ? "true" : "false");
    } else {
        snprintf(response->body, sizeof(response->body),
            "{\n"
            "  \"@odata.type\": \"#SecurityPolicy.v1_0_3.SecurityPolicy\",\n"
            "  \"@odata.id\": \"/redfish/v1/Managers/%s/SecurityPolicy\",\n"
            "  \"Id\": \"SecurityPolicy\",\n"
            "  \"Name\": \"Manager Security Policy\",\n"
            "  \"Description\": \"Security policy for this Manager\",\n"
            "  \"TLS\": {\n"
            "    \"Server\": {\n"
            "      \"Allowed\": {\n"
            "        \"Versions\": [\"1.2\", \"1.3\"]\n"
            "      },\n"
            "      \"VerifyCertificate\": %s,\n"
            "      \"TrustedCertificates\": {\n"
            "        \"@odata.id\": \"/redfish/v1/Managers/Kenmec/SecurityPolicy/TLS/Server/TrustedCertificates\"\n"
            "      }\n"
            "    }\n"
            "  }\n"
            "}",
            manager_id, verify ? "true" : "false");
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    response->content_length = strlen(response->body);
    g_securitypolicy_applytime_onreset = 0;
    return SUCCESS;
}

int post_manager_security_policy(const char *manager_id, const http_request_t *request, http_response_t *response)
{
    if (!manager_id || !request) return ERROR_INVALID_PARAM;
    // Parse minimal fields from request->body
    cJSON *root = cJSON_Parse(request->body);
    security_policy_t policy = { .verify_certificate = 0 };
    if (root) {
        cJSON *tls = cJSON_GetObjectItemCaseSensitive(root, "TLS");
        if (tls) {
            cJSON *server = cJSON_GetObjectItemCaseSensitive(tls, "Server");
            if (server) {
                cJSON *vc = cJSON_GetObjectItemCaseSensitive(server, "VerifyCertificate");
                if (cJSON_IsBool(vc)) policy.verify_certificate = cJSON_IsTrue(vc) ? 1 : 0;
            }
        }
        cJSON_Delete(root);
    }
    if (security_policy_upsert(manager_id, &policy) != 0) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":\"Failed to save SecurityPolicy\"}");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
    g_securitypolicy_applytime_onreset = 1;
    return get_manager_security_policy(manager_id, response);
}

int patch_manager_security_policy(const char *manager_id, const http_request_t *request, http_response_t *response)
{
    return post_manager_security_policy(manager_id, request, response);
}

int delete_manager_security_policy(const char *manager_id, http_response_t *response)
{
    if (!manager_id) return ERROR_INVALID_PARAM;
    if (security_policy_delete(manager_id) != 0) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":\"Failed to delete SecurityPolicy\"}");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
    response->status_code = HTTP_NO_CONTENT;
    response->body[0] = '\0';
    response->content_length = 0;
    return SUCCESS;
}

// POST handlers (create new resources)
int handle_chassis_create(const http_request_t *request, http_response_t *response) {
    if (!request || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll create a new chassis with ID "2"
    // In a real implementation, you would parse the JSON body and validate it
    char *json = generate_chassis_json("2");
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to create chassis\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_CREATED;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int handle_system_create(const http_request_t *request, http_response_t *response) {
    if (!request || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll create a new system with ID "2"
    char *json = generate_system_json("2");
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to create system\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_CREATED;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int handle_manager_create(const http_request_t *request, http_response_t *response) {
    if (!request || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll create a new manager with ID "2"
    char *json = generate_manager_json("2");
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to create manager\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_CREATED;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int handle_accounts_create(const http_request_t *request, http_response_t *response) {
    if (!request || !response) {
        return ERROR_INVALID_PARAM;
    }
    
    const char *body = request->body;
    char *username = NULL;
    char *password = NULL;
    char *role = NULL;
    char default_role = '1';

    cJSON *json = cJSON_Parse(body);
    if (json == NULL) {
        printf("Failed to parse JSON\n");
        return 2;
    }
    cJSON *user = cJSON_GetObjectItemCaseSensitive(json, "UserName");
    cJSON *pass = cJSON_GetObjectItemCaseSensitive(json, "Password");
    cJSON *roleId = cJSON_GetObjectItemCaseSensitive(json, "RoleId");

    if (cJSON_IsString(user) && (user->valuestring != NULL))
        username = user->valuestring;
    if (cJSON_IsString(pass) && (pass->valuestring != NULL))
        password = pass->valuestring;
    if (cJSON_IsString(roleId) && (roleId->valuestring != NULL)) {
        role = roleId->valuestring;
    } else {
        role = &default_role;
    }

    if (!username || !password) {
        printf("Missing fields in JSON\n");
        return ERROR_INVALID_PARAM;
    }

    int rc = account_add(username, password, role, response);
    if(rc != SUCCESS) {
        printf("account_add Failed\n");
        // account_add has already set the response, so return SUCCESS to send it
        cJSON_Delete(json);
        return SUCCESS;
    }

    cJSON_Delete(json);
    return SUCCESS;
}


int handle_accounts(void)
{
    account_get();

    return SUCCESS;
}


int handle_sessions(const http_request_t *request, http_response_t *response)
{
    // POST to create session must be over HTTPS only
    if (!request->is_https) {
        return ERROR_REQUIRES_HTTPS;
    }
    
    const char *body = request->body;
    char *username = NULL;
    char *password = NULL;

    cJSON *json = cJSON_Parse(body);
    if (json == NULL) {
        printf("Failed to parse JSON\n");
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to parse JSON\"}");
        return -1;
    }

    cJSON *user = cJSON_GetObjectItemCaseSensitive(json, "UserName");
    cJSON *pass = cJSON_GetObjectItemCaseSensitive(json, "Password");
    // cJSON *role = cJSON_GetObjectItemCaseSensitive(json, "RoleId");

    if (cJSON_IsString(user) && (user->valuestring != NULL))
        username = user->valuestring;
    if (cJSON_IsString(pass) && (pass->valuestring != NULL))
        password = pass->valuestring;
    // if (cJSON_IsString(role) && (role->valuestring != NULL))
    //     roleId = role->valuestring;

    if (!username || !password) {
        printf("Missing fields in JSON\n");
    }

    db_status_type_t rc = account_check(username, password);
    if (rc != SUCCESS) {
        switch(rc) {
            case DB_STATUS_OPEN_ERROR:
                response->status_code = 500;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, MAX_JSON_SIZE,
                         "{ \"error\": \"Database open error\" }");
                response->content_length = strlen(response->body);
                return SUCCESS;

            case DB_STATUS_PREPARE_ERROR:
                response->status_code = 500;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, MAX_JSON_SIZE,
                         "{ \"error\": \"Database prepare statement error\" }");
                response->content_length = strlen(response->body);
                return SUCCESS;

            case DB_STATUS_PASSWORD_NULL:
                response->status_code = 400;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, MAX_JSON_SIZE,
                         "{ \"error\": \"Password in database is null\" }");
                response->content_length = strlen(response->body);
                return SUCCESS;

            case DB_STATUS_SELECT_ERROR:
                response->status_code = 500;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, MAX_JSON_SIZE,
                         "{ \"error\": \"Database select execution error\" }");
                response->content_length = strlen(response->body);
                return SUCCESS;

            case DB_STATUS_PASSWORD_MISMATCH:
                response->status_code = 401;  // Unauthorized
                strcpy(response->content_type, "application/json");
                snprintf(response->body, MAX_JSON_SIZE,
                         "{ \"error\": \"Password mismatch\" }");
                response->content_length = strlen(response->body);
                
                // Add WWW-Authenticate header for 401 responses
                if (response->header_count < MAX_HEADERS) {
                    strcpy(response->headers[response->header_count][0], "WWW-Authenticate");
                    strcpy(response->headers[response->header_count][1], "Basic realm=\"Redfish\", Bearer");
                    response->header_count++;
                }
                
                return SUCCESS;

            case DB_STATUS_USERNAME_MISMATCH:
                response->status_code = 404; // Not Found
                strcpy(response->content_type, "application/json");
                snprintf(response->body, MAX_JSON_SIZE,
                         "{ \"error\": \"Username not found\" }");
                response->content_length = strlen(response->body);
                return SUCCESS;

            default:
                response->status_code = 500;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, MAX_JSON_SIZE,
                         "{ \"error\": \"Unknown error\" }");
                response->content_length = strlen(response->body);
                return SUCCESS;
        }
    }
    
    char client_token[MAX_TOKEN_LENGTH];
    int session_id;
    rc = session_add(username, client_token, &session_id);
    if(rc != SUCCESS) {
        response->status_code = 500;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, MAX_JSON_SIZE,
                 "{ \"error\": \"Database select execution error\" }");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
    response->status_code = 201; 
    response->header_count = 3;

    strncpy(response->headers[0][0], "X-Auth-Token", MAX_HEADER_NAME_LEN);
    strncpy(response->headers[0][1], client_token, MAX_HEADER_VALUE_LEN);

    strncpy(response->headers[1][0], "Location", MAX_HEADER_NAME_LEN);
    snprintf(response->headers[1][1], MAX_HEADER_VALUE_LEN,
             "/redfish/v1/SessionService/Sessions/%d", session_id);

    strncpy(response->headers[2][0], "Content-Type", MAX_HEADER_NAME_LEN);
    strncpy(response->headers[2][1], "application/json", MAX_HEADER_VALUE_LEN);

    // JSON body: return full representation of the new session resource
    snprintf(response->body, MAX_JSON_SIZE,
        "{"
          "\"@odata.type\":\"#Session.v1_2_0.Session\"," 
          "\"@odata.id\":\"/redfish/v1/SessionService/Sessions/%d\"," 
          "\"Id\":\"%d\"," 
          "\"Name\":\"User Session\"," 
          "\"UserName\":\"%s\""
        "}",
        session_id, session_id, username);

    response->content_length = strlen(response->body);
    
    return SUCCESS;
}

int handle_session_delete(const char *token, http_response_t *response)
{
    if (!token || !response) {
        return ERROR_INVALID_PARAM;
    }

    int rc = session_delete(token);
    if (rc == SUCCESS) {
        response->status_code = HTTP_NO_CONTENT;
        response->body[0] = '\0';
        response->content_length = 0;
        return SUCCESS;
    }

    response->status_code = HTTP_NOT_FOUND;
    strcpy(response->content_type, "application/json");
    snprintf(response->body, sizeof(response->body),
             "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"Session not found\"}}"
    );
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_account_delete(const char *account_id_str, http_response_t *response)
{
    if (!account_id_str || !response) {
        return ERROR_INVALID_PARAM;
    }

    // Parse account ID
    char *endptr;
    long account_id = strtol(account_id_str, &endptr, 10);
    if (*endptr != '\0' || account_id <= 0) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
                 "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"Invalid account ID\"}}"
        );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Prevent deletion of account ID 1 (admin account)
    if (account_id == 1) {
        response->status_code = HTTP_FORBIDDEN;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
                 "{\"error\":{\"code\":\"Base.1.15.0.ActionNotSupported\",\"message\":\"Cannot delete admin account (ID 1)\"}}"
        );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    int rc = account_delete((int)account_id);
    if (rc == SUCCESS) {
        response->status_code = HTTP_NO_CONTENT;
        response->body[0] = '\0';
        response->content_length = 0;
        return SUCCESS;
    } else {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
                 "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"Account not found\"}}"
        );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
}


// Return a Session member resource by ID
int handle_session_member(const char *session_id_str, http_response_t *response)
{
    if (!session_id_str || !response) {
        return ERROR_INVALID_PARAM;
    }

    char *endptr = NULL;
    long session_id = strtol(session_id_str, &endptr, 10);
    if (session_id <= 0 || (endptr && *endptr != '\0')) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
            "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/SessionService/Sessions/%s was not found.\"}}",
            session_id_str);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Query all sessions and find the one with matching id
    int total = session_count_get();
    if (total <= 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
            "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/SessionService/Sessions/%s was not found.\"}}",
            session_id_str);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    session_info_t *sessions = calloc(total, sizeof(session_info_t));
    if (!sessions) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body), "{ \"error\": \"Memory allocation failed\" }");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    int count = session_list_get(sessions);
    if (count < 0) {
        free(sessions);
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body), "{ \"error\": \"Failed to retrieve sessions from database\" }");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    const session_info_t *found = NULL;
    for (int i = 0; i < count; i++) {
        if (sessions[i].id == (int)session_id) {
            found = &sessions[i];
            break;
        }
    }

    if (!found) {
        free(sessions);
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
            "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/SessionService/Sessions/%s was not found.\"}}",
            session_id_str);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    snprintf(response->body, sizeof(response->body),
        "{"
          "\"@odata.type\":\"#Session.v1_2_0.Session\"," 
          "\"@odata.id\":\"/redfish/v1/SessionService/Sessions/%d\"," 
          "\"Id\":\"%d\"," 
          "\"Name\":\"User Session\"," 
          "\"UserName\":\"%s\""
        "}",
        found->id, found->id, found->username[0] ? found->username : "");
    response->content_length = strlen(response->body);

    free(sessions);
    return SUCCESS;
}




// PUT handlers (update existing resources)
int handle_chassis_update(const char *chassis_id, const http_request_t *request, http_response_t *response) {
    if (!chassis_id || !request || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll return the updated chassis data
    // In a real implementation, you would parse the JSON body and update the resource
    char *json = generate_chassis_json(chassis_id);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to update chassis\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int handle_system_update(const char *system_id, const http_request_t *request, http_response_t *response) {
    if (!system_id || !request || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll return the updated system data
    char *json = generate_system_json(system_id);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to update system\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}

int handle_manager_update(const char *manager_id, const http_request_t *request, http_response_t *response) {
    if (!manager_id || !request || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll return the updated manager data
    char *json = generate_manager_json(manager_id);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to update manager\"}");
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->body, json);
    response->content_length = strlen(json);
    free(json);

    return SUCCESS;
}
// PATCH handlers (modify items)
int patch_managers_ethernet_interface_eth0(const char *resource_id, const http_request_t *request, http_response_t *response) {
    char *json = open_json_file(resource_id, MANAGER_ETHERNET_INTERFACE_ETH0);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to generate managers collection\"}");
        return ERROR_MEMORY;
    }

    printf("request->body = %s\n", request->body);

    cJSON *root = cJSON_Parse(json);
    cJSON *patch_obj = cJSON_Parse(request->body);
    if (!root || !patch_obj) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->body, "{\"error\":\"Invalid JSON format\"}");
        free(json);
        return -1;
    }

    // Parse IPv4 configuration from the PATCH request
    NetUtilitiesEthernetConfig config = {0}; // Initialize to zero
    
    // Extract IPv4Addresses array from the patch
    cJSON *ipv4_addresses = cJSON_GetObjectItem(patch_obj, "IPv4Addresses");
    if (ipv4_addresses && cJSON_IsArray(ipv4_addresses)) {
        cJSON *first_address = cJSON_GetArrayItem(ipv4_addresses, 0);
        if (first_address) {
            // Extract Address
            cJSON *address = cJSON_GetObjectItem(first_address, "Address");
            if (address && cJSON_IsString(address)) {
                printf("Setting IP address: %s\n", address->valuestring);
                // Convert IP string to byte array
                if (inet_pton(AF_INET, address->valuestring, config.address) != 1) {
                    printf("Error: Invalid IP address format: %s\n", address->valuestring);
                }
            }
            
            // Extract SubnetMask
            cJSON *subnet_mask = cJSON_GetObjectItem(first_address, "SubnetMask");
            if (subnet_mask && cJSON_IsString(subnet_mask)) {
                printf("Setting subnet mask: %s\n", subnet_mask->valuestring);
                if (inet_pton(AF_INET, subnet_mask->valuestring, config.subnet_mask) != 1) {
                    printf("Error: Invalid subnet mask format: %s\n", subnet_mask->valuestring);
                }
            }
            
            // Extract Gateway
            cJSON *gateway = cJSON_GetObjectItem(first_address, "Gateway");
            if (gateway && cJSON_IsString(gateway)) {
                printf("Setting gateway: %s\n", gateway->valuestring);
                if (inet_pton(AF_INET, gateway->valuestring, config.gateway) != 1) {
                    printf("Error: Invalid gateway format: %s\n", gateway->valuestring);
                }
            }
            
            // Extract AddressOrigin to determine DHCP vs Static
            cJSON *address_origin = cJSON_GetObjectItem(first_address, "AddressOrigin");
            if (address_origin && cJSON_IsString(address_origin)) {
                if (strcmp(address_origin->valuestring, "DHCP") == 0) {
                    config.is_dhcp = 1;
                    printf("Setting DHCP mode\n");
                } else {
                    config.is_dhcp = 0;
                    printf("Setting Static IP mode\n");
                }
            } else {
                // Default to static if not specified
                config.is_dhcp = 0;
                printf("Defaulting to Static IP mode\n");
            }
            
            // Set other flags
            config.ipv6 = 0; // We're only handling IPv4 for now
            config.cable_detected = 1; // Assume cable is detected
            
            printf("Ethernet config - IP: %d.%d.%d.%d, Mask: %d.%d.%d.%d, Gateway: %d.%d.%d.%d, DHCP: %s\n",
                   config.address[0], config.address[1], config.address[2], config.address[3],
                   config.subnet_mask[0], config.subnet_mask[1], config.subnet_mask[2], config.subnet_mask[3],
                   config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3],
                   config.is_dhcp ? "Yes" : "No");
            
            // Store configuration for delayed application (after response is sent)
            printf("Network configuration prepared - will apply after response is sent\n");
            
            // Schedule delayed network configuration (3 seconds delay)
            printf("Scheduling delayed network configuration (3 seconds delay)...\n");
            schedule_delayed_network_config(config, 3); // 3 seconds delay
        } else {
            printf("Warning: No IPv4 address found in the patch request\n");
        }
    } else {
        printf("Warning: No IPv4Addresses array found in the patch request\n");
    }

    cjson_deep_merge(root, patch_obj);

    char *modified = cJSON_Print(root);

    strcpy(response->body, modified);
    response->status_code = HTTP_OK;
    response->content_length = strlen(modified);

    save_json_file(resource_id, modified);

    free(json);
    cJSON_Delete(root);
    cJSON_Delete(patch_obj);
    free(modified);

    return SUCCESS;
}

int handle_manager_reset_action(const char *manager_id, const http_request_t *request, http_response_t *response) {
    if (!manager_id || !request || !response) return ERROR_INVALID_PARAM;

    // Validate manager ID
    if (strcmp(manager_id, MANAGER_ID_KENMEC) != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
            "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/Managers/%s was not found.\"}}",
            manager_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Parse the request body to get ResetType
    cJSON *json = cJSON_Parse(request->body);
    if (!json) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.MalformedJSON\",\"message\":\"The request body contains malformed JSON.\"}}");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    cJSON *reset_type = cJSON_GetObjectItem(json, "ResetType");
    if (!reset_type || !cJSON_IsString(reset_type)) {
        cJSON_Delete(json);
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.PropertyMissing\",\"message\":\"The property ResetType is required in the request body.\"}}");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    const char *reset_type_str = cJSON_GetStringValue(reset_type);
    
    // Validate ResetType - only ForceRestart is allowed based on the Actions definition
    if (strcmp(reset_type_str, "ForceRestart") != 0) {
        cJSON_Delete(json);
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
            "{\"error\":{\"code\":\"Base.1.15.0.PropertyValueNotInList\",\"message\":\"The value '%s' for the property ResetType is not in the list of valid values.\"}}",
            reset_type_str);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // TODO: Implement actual reset logic here
    // For now, just return success
    printf("Manager.Reset action called for manager %s with ResetType: %s\n", manager_id, reset_type_str);
    
    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    strcpy(response->body, "{\"message\":\"Manager reset initiated successfully\"}");
    response->content_length = strlen(response->body);
    response->post_action = LABEL_POST_ACTION_FORCE_RESTART;

    cJSON_Delete(json);

    return SUCCESS;
}



// ThermalEquipment JSON generation functions
char* generate_thermalequipment_collection_json(void) {
    char *json = malloc(1024);
    if (!json) return NULL;

    snprintf(json, 1024,
        "{"
        "\"@odata.type\":\"#ThermalEquipment.v1_2_0.ThermalEquipment\","
        "\"@odata.id\":\"/redfish/v1/ThermalEquipment\","
        "\"Id\":\"ThermalEquipment\","
        "\"Name\":\"Cooling Equipment\","
        "\"Description\":\"Thermal management and cooling equipment\","
        "\"Status\":{"
        "\"State\":\"Enabled\","
        "\"Health\":\"OK\""
        "},"
        "\"CDUs\":{"
        "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs\""
        "}"
        "}");

    return json;
}

char* generate_thermalequipment_json(const char *thermalequipment_id) {
    if (!thermalequipment_id) return NULL;

    char *json = malloc(2048);
    if (!json) return NULL;

    // Handle CDUs as a collection
    if (strcmp(thermalequipment_id, "CDUs") == 0) {
        snprintf(json, 2048,
            "{"
            "\"@odata.type\":\"#CoolingUnitCollection.CoolingUnitCollection\","
            "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs\","
            "\"Name\":\"CDU Collection\","
            "\"Description\":\"Collection of all Coolant Distribution Units\","
            "\"Members@odata.count\":1,"
            "\"Members\":["
            "{\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/1\"}"
            "]"
            "}");
    }
    // Handle individual CDU (e.g., CDUs/1)
    else if (strncmp(thermalequipment_id, "CDUs/", 5) == 0) {
        const char *cdu_id = thermalequipment_id + 5; // Skip "CDUs/"
        
        // Validate CDU existence - only CDU "1" exists in this demo
        if (strcmp(cdu_id, "1") != 0) {
            // CDU doesn't exist, return NULL to indicate 404 handled by caller
            free(json);
            return NULL;
        }
        
        snprintf(json, 2048,
            "{"
            "\"@odata.type\":\"#CoolingUnit.v1_3_0.CoolingUnit\","
            "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s\","
            "\"Id\":\"%s\","
            "\"Name\":\"CDU %s\","
            "\"EquipmentType\":\"CDU\","
            "\"Manufacturer\":\"Kenmec\","
            "\"Status\":{"
            "\"State\":\"Enabled\","
            "\"Health\":\"OK\""
            "},"
            "\"Coolant\":{"
            "\"CoolantType\":\"Water\""
            "},"
            "\"Oem\":{"
            "\"Kenmec\":{"
            "\"IOBoards\":{\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards\"},"
            "\"Config\":{\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/Config\"},"
            "\"ControlLogics\":{\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics\"}"
            "}"
            "}"
            "}",
            cdu_id, cdu_id, cdu_id, cdu_id, cdu_id, cdu_id);
    }

    return json;
}

// ThermalEquipment handlers
int handle_thermalequipment_collection(http_response_t *response) {
    if (!response) {
        return ERROR_INVALID_PARAM;
    }

    char *json = generate_thermalequipment_collection_json();
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "Failed to generate ThermalEquipment collection JSON");
        response->content_length = strlen(response->body);
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");
    strcpy(response->body, json);
    response->content_length = strlen(response->body);
    
    free(json);
    return SUCCESS;
}

int handle_thermalequipment(const char *thermalequipment_id, http_response_t *response) {
    if (!thermalequipment_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    char *json = generate_thermalequipment_json(thermalequipment_id);
    if (!json) {
        // Return a general Redfish-compliant 404 for any unsupported or missing ThermalEquipment subresource
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body),
            "{"
              "\"error\":{"
                "\"code\":\"Base.1.15.0.ResourceMissingAtURI\","
                "\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/%s was not found.\","
                "\"@Message.ExtendedInfo\":[{"
                  "\"@odata.type\":\"#Message.v1_1_1.Message\","
                  "\"MessageId\":\"Base.1.15.0.ResourceMissingAtURI\","
                  "\"Message\":\"The resource at the URI /redfish/v1/ThermalEquipment/%s was not found.\","
                  "\"MessageArgs\":[\"/redfish/v1/ThermalEquipment/%s\"],"
                  "\"Severity\":\"Critical\","
                  "\"Resolution\":\"Provide a valid URI and resubmit the request.\""
                "}]"
              "}"
            "}",
            thermalequipment_id, thermalequipment_id, thermalequipment_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");
    strcpy(response->body, json);
    response->content_length = strlen(response->body);
    
    free(json);
    return SUCCESS;
}

// ThermalEquipment POST handler (create new thermal equipment)
int handle_thermalequipment_create(const http_request_t *request, http_response_t *response) {
    if (!request || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll create a new thermal equipment with ID "ThermalEquipment3"
    char *json = generate_thermalequipment_json("ThermalEquipment3");
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "Failed to create ThermalEquipment");
        response->content_length = strlen(response->body);
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_CREATED;
    strcpy(response->content_type, "application/json");
    strcpy(response->body, json);
    response->content_length = strlen(response->body);
    
    // Add Location header
    response->header_count = 1;
    strcpy(response->headers[0][0], "Location");
    strcpy(response->headers[0][1], "/redfish/v1/ThermalEquipment/ThermalEquipment3");
    
    free(json);
    return SUCCESS;
}

// ThermalEquipment PUT handler (update existing thermal equipment)
int handle_thermalequipment_update(const char *thermalequipment_id, const http_request_t *request, http_response_t *response) {
    if (!thermalequipment_id || !request || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll return the updated thermal equipment
    char *json = generate_thermalequipment_json(thermalequipment_id);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "Failed to update ThermalEquipment");
        response->content_length = strlen(response->body);
        return ERROR_MEMORY;
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");
    strcpy(response->body, json);
    response->content_length = strlen(response->body);
    
    free(json);
    return SUCCESS;
}

// ThermalEquipment DELETE handler (remove thermal equipment)
int handle_thermalequipment_delete(const char *thermalequipment_id, http_response_t *response) {
    if (!thermalequipment_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll return a success response
    // In a real implementation, you would actually remove the resource
    response->status_code = HTTP_NO_CONTENT;
    strcpy(response->body, "");
    response->content_length = 0;

    return SUCCESS;
}

// SessionService handler
int handle_session_service(http_response_t *response) {
    if (!response) {
        return ERROR_INVALID_PARAM;
    }

    const char *odata_type = "#SessionService.v1_2_0.SessionService";
    const char *odata_id = "/redfish/v1/SessionService";
    const char *sessions_link = "/redfish/v1/SessionService/Sessions";

    snprintf(response->body, sizeof(response->body),
        "{"
          "\"@odata.type\":\"%s\"," 
          "\"@odata.id\":\"%s\"," 
          "\"Id\":\"SessionService\"," 
          "\"Name\":\"Session Service\"," 
          "\"Status\":{\"State\":\"Enabled\",\"Health\":\"OK\"},"
          "\"SessionTimeout\": %d,"
          "\"ServiceEnabled\": true,"
          "\"Sessions\":{\"@odata.id\":\"%s\"}"
        "}",
        odata_type, odata_id, SESSION_EXPIRY_SECONDS, sessions_link);

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    response->content_length = strlen(response->body);
    return SUCCESS;
}

// TODO: Need to revise to dynamic sessions collection.
int handle_sessions_collection(const http_request_t *request, http_response_t *response)
{
    if (!request || !response) return ERROR_INVALID_PARAM;
    
    // Check if HTTP request with Basic Auth - should redirect to HTTPS
    if (!request->is_https) {
        for (int i = 0; i < request->header_count; i++) {
            if (strcasecmp(request->headers[i][0], "Authorization") == 0) {
                const char *auth_value = request->headers[i][1];
                if (strncasecmp(auth_value, "Basic ", 6) == 0) {
                    return ERROR_REQUIRES_HTTPS;
                }
            }
        }
    }
    
    // Get session count and list from database
    int total_sessions = session_count_get();
    session_info_t *sessions = calloc(total_sessions, sizeof(session_info_t));
    if (!sessions) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
            "{ \"error\": \"Memory allocation failed\" }");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
    
    int session_count = session_list_get(sessions);
    
    if (session_count < 0) {
        free(sessions);
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        snprintf(response->body, sizeof(response->body),
            "{ \"error\": \"Failed to retrieve sessions from database\" }");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    
    // Build Members array
    char members_json[MAX_JSON_SIZE] = "";
    for (int i = 0; i < session_count; i++) {
        if (i > 0) {
            strcat(members_json, ",");
        }
        char member_entry[256];
        snprintf(member_entry, sizeof(member_entry),
            "{\"@odata.id\":\"/redfish/v1/SessionService/Sessions/%d\"}",
            sessions[i].id);
        strcat(members_json, member_entry);
    }
    
    snprintf(response->body, sizeof(response->body),
        "{"
          "\"@odata.type\":\"#SessionCollection.SessionCollection\"," 
          "\"@odata.id\":\"/redfish/v1/SessionService/Sessions\"," 
          "\"Name\":\"Session Collection\"," 
          "\"Members@odata.count\":%d," 
          "\"Members\":[%s]"
        "}",
        session_count, members_json);
    response->content_length = strlen(response->body);
    
    free(sessions);
    return SUCCESS;
}

// CDU OEM Config handler
int handle_cdu_oem_kenmec_config_read(const char *cdu_id, http_response_t *response) {
    if (!response) {
        return ERROR_INVALID_PARAM;
    }

    // Validate CDU and member
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s was not found.\"}}", cdu_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Build success response
    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");

    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddStringToObject(response_json, "@odata.type", "#KenmecConfig.v1_0_0.KenmecConfig");
    
    char odata_id[256];
    snprintf(odata_id, sizeof(odata_id), "/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/Config.Read", cdu_id);
    cJSON_AddStringToObject(response_json, "@odata.id", odata_id);

    int config_count = 0;

    // "SystemConfigs"
    cJSON *system_configs = cJSON_CreateObject();
    system_config_t *system_config = control_logic_system_configs_get();
    if (system_config != NULL) {
        if (strlen(system_config->machine_type) > 0) {
            cJSON_AddStringToObject(system_configs, "machine_type", system_config->machine_type);
        }
    }
    cJSON_AddItemToObject(response_json, "SystemConfigs", system_configs);

    // "ModbusDeviceConfigs"
    cJSON *modbus_device_config_array = cJSON_CreateArray();
    // modbus device config to json
    modbus_device_config_t *modbus_device_config = control_logic_modbus_device_configs_get(&config_count);
    if (modbus_device_config != NULL && config_count > 0) {
        for (int i = 0; i < config_count; i++) {
            cJSON *config_item = cJSON_CreateObject();
            cJSON_AddNumberToObject(config_item, "board", modbus_device_config[i].port);
            cJSON_AddNumberToObject(config_item, "baudrate", modbus_device_config[i].baudrate);
            cJSON_AddNumberToObject(config_item, "slave_id", modbus_device_config[i].slave_id);
            cJSON_AddNumberToObject(config_item, "code", modbus_device_config[i].function_code);
            cJSON_AddNumberToObject(config_item, "address", modbus_device_config[i].reg_address);
            cJSON_AddNumberToObject(config_item, "data_type", modbus_device_config[i].data_type);
            cJSON_AddNumberToObject(config_item, "scale", modbus_device_config[i].fScale);
            cJSON_AddNumberToObject(config_item, "update_address", modbus_device_config[i].update_address);
            cJSON_AddStringToObject(config_item, "name", modbus_device_config[i].name);
            cJSON_AddItemToArray(modbus_device_config_array, config_item);
        }
    }
    cJSON_AddItemToObject(response_json, "ModbusDeviceConfigs", modbus_device_config_array);

    // "TemperatureConfigs"
    cJSON *temperature_config_array = cJSON_CreateArray();
    temperature_config_t *temperature_config = control_logic_temperature_configs_get(&config_count);
    if (temperature_config != NULL && config_count > 0) {
        for (int i = 0; i < config_count; i++) {
            cJSON *config_item = cJSON_CreateObject();
            cJSON_AddNumberToObject(config_item, "board", temperature_config[i].port);
            cJSON_AddNumberToObject(config_item, "channel", temperature_config[i].channel);
            cJSON_AddNumberToObject(config_item, "sensor_type", temperature_config[i].sensor_type);
            cJSON_AddNumberToObject(config_item, "update_address", temperature_config[i].update_address);
            cJSON_AddStringToObject(config_item, "name", temperature_config[i].name);
            cJSON_AddItemToArray(temperature_config_array, config_item);
        }
    }
    cJSON_AddItemToObject(response_json, "TemperatureConfigs", temperature_config_array);

    // "AnalogInputCurrentConfigs"
    cJSON *analog_input_current_config_array = cJSON_CreateArray();
    analog_config_t *analog_input_current_config = control_logic_analog_input_current_configs_get(&config_count);
    if (analog_input_current_config != NULL && config_count > 0) {
        for (int i = 0; i < config_count; i++) {
            cJSON *config_item = cJSON_CreateObject();
            cJSON_AddNumberToObject(config_item, "board", analog_input_current_config[i].port);
            cJSON_AddNumberToObject(config_item, "channel", analog_input_current_config[i].channel);
            cJSON_AddNumberToObject(config_item, "sensor_type", analog_input_current_config[i].sensor_type);
            cJSON_AddNumberToObject(config_item, "update_address", analog_input_current_config[i].update_address);
            cJSON_AddStringToObject(config_item, "name", analog_input_current_config[i].name);
            cJSON_AddItemToArray(analog_input_current_config_array, config_item);
        }
    }
    cJSON_AddItemToObject(response_json, "AnalogInputCurrentConfigs", analog_input_current_config_array);

    // "AnalogInputVoltageConfigs"
    cJSON *analog_input_voltage_config_array = cJSON_CreateArray();
    analog_config_t *analog_input_voltage_config = control_logic_analog_input_voltage_configs_get(&config_count);
    if (analog_input_voltage_config != NULL && config_count > 0) {
        for (int i = 0; i < config_count; i++) {
            cJSON *config_item = cJSON_CreateObject();
            cJSON_AddNumberToObject(config_item, "board", analog_input_voltage_config[i].port);
            cJSON_AddNumberToObject(config_item, "channel", analog_input_voltage_config[i].channel);
            cJSON_AddNumberToObject(config_item, "sensor_type", analog_input_voltage_config[i].sensor_type);
            cJSON_AddNumberToObject(config_item, "update_address", analog_input_voltage_config[i].update_address);
            cJSON_AddStringToObject(config_item, "name", analog_input_voltage_config[i].name);
            cJSON_AddItemToArray(analog_input_voltage_config_array, config_item);
        }
    }
    cJSON_AddItemToObject(response_json, "AnalogInputVoltageConfigs", analog_input_voltage_config_array);

    // "AnalogOutputCurrentConfigs"
    cJSON *analog_output_current_config_array = cJSON_CreateArray();
    analog_config_t *analog_output_current_config = control_logic_analog_output_current_configs_get(&config_count);
    if (analog_output_current_config != NULL && config_count > 0) {
        for (int i = 0; i < config_count; i++) {
            cJSON *config_item = cJSON_CreateObject();
            cJSON_AddNumberToObject(config_item, "board", analog_output_current_config[i].port);
            cJSON_AddNumberToObject(config_item, "channel", analog_output_current_config[i].channel);
            cJSON_AddNumberToObject(config_item, "sensor_type", analog_output_current_config[i].sensor_type);
            cJSON_AddStringToObject(config_item, "name", analog_output_current_config[i].name);
            cJSON_AddItemToArray(analog_output_current_config_array, config_item);
        }
    }
    cJSON_AddItemToObject(response_json, "AnalogOutputCurrentConfigs", analog_output_current_config_array);

    // "AnalogOutputVoltageConfigs"
    cJSON *analog_output_voltage_config_array = cJSON_CreateArray();
    analog_config_t *analog_output_voltage_config = control_logic_analog_output_voltage_configs_get(&config_count);
    if (analog_output_voltage_config != NULL && config_count > 0) {
        for (int i = 0; i < config_count; i++) {
            cJSON *config_item = cJSON_CreateObject();
            cJSON_AddNumberToObject(config_item, "board", analog_output_voltage_config[i].port);
            cJSON_AddNumberToObject(config_item, "channel", analog_output_voltage_config[i].channel);
            cJSON_AddNumberToObject(config_item, "sensor_type", analog_output_voltage_config[i].sensor_type);
            cJSON_AddStringToObject(config_item, "name", analog_output_voltage_config[i].name);
            cJSON_AddItemToArray(analog_output_voltage_config_array, config_item);
        }
    }
    cJSON_AddItemToObject(response_json, "AnalogOutputVoltageConfigs", analog_output_voltage_config_array);

    // json to string
    char *json_string = cJSON_PrintUnformatted(response_json);
    if (json_string) {
        // debug(tag, "json_string length: %d", strlen(json_string));
        // debug(tag, "response->body: %d", sizeof(response->body));
        sprintf(response->body, "%s", json_string);
        // debug(tag, "response->body: %d", strlen(response->body));
        free(json_string);
    }
    cJSON_Delete(response_json);
   
    response->content_length = strlen(response->body);

    return SUCCESS;
}


int handle_cdu_oem_kenmec_config_write(const char *cdu_id, http_request_t *request, http_response_t *response) {
    int ret = SUCCESS;

    if (!response) {
        return ERROR_INVALID_PARAM;
    }

    // Validate CDU and member
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s was not found.\"}}", cdu_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Parse request JSON
    cJSON *root = cJSON_Parse(request->body);
    if (!root) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.ActionParameterMissing\",\"message\":\"Invalid JSON.\"}}" );
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Parse "Machine" key and set to file
    cJSON *jsonSystemConfigs = cJSON_GetObjectItemCaseSensitive(root, "SystemConfigs");
    if (jsonSystemConfigs && cJSON_IsObject(jsonSystemConfigs)) {
        char *json_string = cJSON_PrintUnformatted(jsonSystemConfigs);
        if (json_string) {
            ret = control_logic_system_configs_set(json_string);
            if (ret != SUCCESS) {
                free(json_string);
                cJSON_Delete(root);
                response->status_code = HTTP_BAD_REQUEST;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.InternalError\",\"message\":\"Failed to save SystemConfigs.\"}}");
                response->content_length = strlen(response->body);
                return SUCCESS;
            }
            free(json_string);
        }
    }

    // "ModbusDeviceConfigs" array
    cJSON *jsonModbusDeviceConfigs = cJSON_GetObjectItemCaseSensitive(root, "ModbusDeviceConfigs");
    if (jsonModbusDeviceConfigs && cJSON_IsArray(jsonModbusDeviceConfigs)) {
        // apply modbus device config
        char *json_string = cJSON_PrintUnformatted(jsonModbusDeviceConfigs);
        if (json_string) {
            ret = control_logic_modbus_device_configs_set(json_string);
            if (ret != SUCCESS) {
                free(json_string);
                cJSON_Delete(root);
                response->status_code = HTTP_BAD_REQUEST;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.InternalError\",\"message\":\"Failed to serialize JSON configuration.\"}}" );
                response->content_length = strlen(response->body);
                return SUCCESS;
            }
            free(json_string);
        }
    }

    // "TemperatureConfigs"
    cJSON *jsonTemperatureConfigs = cJSON_GetObjectItemCaseSensitive(root, "TemperatureConfigs");
    if (jsonTemperatureConfigs && cJSON_IsArray(jsonTemperatureConfigs)) {
        char *json_string = cJSON_PrintUnformatted(jsonTemperatureConfigs);
        if (json_string) {
            ret = control_logic_temperature_configs_set(json_string);
            if (ret != SUCCESS) {
                free(json_string);
                cJSON_Delete(root);
                response->status_code = HTTP_BAD_REQUEST;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.InternalError\",\"message\":\"Failed to set %s.\"}}" , jsonTemperatureConfigs->valuestring);
                response->content_length = strlen(response->body);
                return SUCCESS;
            }
            free(json_string);
        }
    }

    // "AnalogInputCurrentConfigs"
    cJSON *jsonAnalogInputCurrentConfigs = cJSON_GetObjectItemCaseSensitive(root, "AnalogInputCurrentConfigs");
    if (jsonAnalogInputCurrentConfigs && cJSON_IsArray(jsonAnalogInputCurrentConfigs)) {
        char *json_string = cJSON_PrintUnformatted(jsonAnalogInputCurrentConfigs);
        if (json_string) {
            ret = control_logic_analog_input_current_configs_set(json_string);
            if (ret != SUCCESS) {
                free(json_string);
                cJSON_Delete(root);
                response->status_code = HTTP_BAD_REQUEST;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.InternalError\",\"message\":\"Failed to set %s.\"}}" , jsonAnalogInputCurrentConfigs->valuestring);
                response->content_length = strlen(response->body);
                return SUCCESS;
            }
            free(json_string);
        }
    }

    // "AnalogInputVoltageConfigs"
    cJSON *jsonAnalogInputVoltageConfigs = cJSON_GetObjectItemCaseSensitive(root, "AnalogInputVoltageConfigs");
    if (jsonAnalogInputVoltageConfigs && cJSON_IsArray(jsonAnalogInputVoltageConfigs)) {
        char *json_string = cJSON_PrintUnformatted(jsonAnalogInputVoltageConfigs);
        if (json_string) {
            ret = control_logic_analog_input_voltage_configs_set(json_string);
            if (ret != SUCCESS) {
                free(json_string);
                cJSON_Delete(root);
                response->status_code = HTTP_BAD_REQUEST;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.InternalError\",\"message\":\"Failed to set %s.\"}}" , jsonAnalogInputCurrentConfigs->valuestring);
                response->content_length = strlen(response->body);
                return SUCCESS;
            }
            free(json_string);
        }
    }

    // "AnalogOutputVoltageConfigs"
    cJSON *jsonAnalogOutputVoltageConfigs = cJSON_GetObjectItemCaseSensitive(root, "AnalogOutputVoltageConfigs");
    if (jsonAnalogOutputVoltageConfigs && cJSON_IsArray(jsonAnalogOutputVoltageConfigs)) {
        char *json_string = cJSON_PrintUnformatted(jsonAnalogOutputVoltageConfigs);
        if (json_string) {
            ret = control_logic_analog_output_voltage_configs_set(json_string);
            if (ret != SUCCESS) {
                free(json_string);
                cJSON_Delete(root);
                response->status_code = HTTP_BAD_REQUEST;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.InternalError\",\"message\":\"Failed to set %s.\"}}" , jsonAnalogInputCurrentConfigs->valuestring);
                response->content_length = strlen(response->body);
                return SUCCESS;
            }
            free(json_string);
        }
    }

    // "AnalogOutputCurrentConfigs"
    cJSON *jsonAnalogOutputCurrentConfigs = cJSON_GetObjectItemCaseSensitive(root, "AnalogOutputCurrentConfigs");
    if (jsonAnalogOutputCurrentConfigs && cJSON_IsArray(jsonAnalogOutputCurrentConfigs)) {
        char *json_string = cJSON_PrintUnformatted(jsonAnalogOutputCurrentConfigs);
        if (json_string) {
            ret = control_logic_analog_output_current_configs_set(json_string);
            if (ret != SUCCESS) {
                free(json_string);
                cJSON_Delete(root);
                response->status_code = HTTP_BAD_REQUEST;
                strcpy(response->content_type, "application/json");
                snprintf(response->body, sizeof(response->body), "{\"error\":{\"code\":\"Base.1.15.0.InternalError\",\"message\":\"Failed to set %s.\"}}" , jsonAnalogInputCurrentConfigs->valuestring);
                response->content_length = strlen(response->body);
                return SUCCESS;
            }
            free(json_string);
        }
    }

    // free root
    cJSON_Delete(root);

    // Build success response
    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");

    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddStringToObject(response_json, "@odata.type", "#KenmecConfig.v1_0_0.KenmecConfig");
    
    char odata_id[512];
    snprintf(odata_id, sizeof(odata_id), "/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/Config.Write",  cdu_id);

    cJSON_AddStringToObject(response_json, "@odata.id", odata_id);
    cJSON_AddStringToObject(response_json, "Status", "Success");
    
    char *json_string = cJSON_Print(response_json);
    if (json_string) {
        strncpy(response->body, json_string, sizeof(response->body) - 1);
        response->body[sizeof(response->body) - 1] = '\0';
        free(json_string);
    }
    cJSON_Delete(response_json);

    response->content_length = strlen(response->body);

    return SUCCESS;
}

// CDU OEM IOBoards handler
int handle_cdu_oem(const char *cdu_id, http_response_t *response) {
    if (!cdu_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    // Validate CDU existence - only CDU "1" exists in this demo
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body),
            "{"
              "\"error\":{"
                "\"code\":\"Base.1.15.0.ResourceMissingAtURI\","
                "\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem was not found.\","
                "\"@Message.ExtendedInfo\":[{"
                  "\"@odata.type\":\"#Message.v1_1_1.Message\","
                  "\"MessageId\":\"Base.1.15.0.ResourceMissingAtURI\","
                  "\"Message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem was not found.\","
                  "\"MessageArgs\":[\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem\"],"
                  "\"Severity\":\"Critical\","
                  "\"Resolution\":\"Provide a valid URI and resubmit the request.\""
                "}]"
              "}"
            "}",
            cdu_id, cdu_id, cdu_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Generate OEM resource JSON
    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");
    
    snprintf(response->body, sizeof(response->body),
        "{"
        "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem\"," 
        "\"Kenmec\":{"
            "\"@odata.type\":\"#KenmecCDUOem.v1_0_0.KenmecCDUOem\"," 
            "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec\"," 
            "\"IOBoards\":{"
                "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards\""
            "}"
            "\"Config\":{"
                "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/Config\""
            "}"
            "\"ControlLogics\":{"
                "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics\""
            "}"
        "}"
        "}",
        cdu_id, cdu_id, cdu_id, cdu_id, cdu_id);
    
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_cdu_oem_kenmec(const char *cdu_id, http_response_t *response) {
    if (!cdu_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    // Validate CDU existence - only CDU "1" exists in this demo
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body),
            "{"
              "\"error\":{"
                "\"code\":\"Base.1.15.0.ResourceMissingAtURI\","
                "\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec was not found.\","
                "\"@Message.ExtendedInfo\":[{"
                  "\"@odata.type\":\"#Message.v1_1_1.Message\","
                  "\"MessageId\":\"Base.1.15.0.ResourceMissingAtURI\","
                  "\"Message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec was not found.\","
                  "\"MessageArgs\":[\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec\"],"
                  "\"Severity\":\"Critical\","
                  "\"Resolution\":\"Provide a valid URI and resubmit the request.\""
                "}]"
              "}"
            "}",
            cdu_id, cdu_id, cdu_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");

    snprintf(response->body, sizeof(response->body),
        "{"
        "\"@odata.type\":\"#KenmecCDUOem.v1_0_0.KenmecCDUOem\","
        "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec\","
        "\"Name\":\"Kenmec OEM Extensions\","
        "\"Description\":\"Kenmec-specific extensions for CDU resource\","
        "\"IOBoards\":{\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards\"},"
        "\"Config\":{\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/Config\"},"
        "\"ControlLogics\":{\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics\"}"
        "}",
        cdu_id, cdu_id, cdu_id, cdu_id);
    response->content_length = strlen(response->body);
    return SUCCESS;
}


int handle_cdu_oem_control_logics(const char *cdu_id, http_response_t *response) {
    if (!cdu_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    // Validate CDU existence - only CDU "1" exists in this demo
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body),
            "{"
              "\"error\":{"
                "\"code\":\"Base.1.15.0.ResourceMissingAtURI\","
                "\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards was not found.\","
                "\"@Message.ExtendedInfo\":[{"
                  "\"@odata.type\":\"#Message.v1_1_1.Message\","
                  "\"MessageId\":\"Base.1.15.0.ResourceMissingAtURI\","
                  "\"Message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards was not found.\","
                  "\"MessageArgs\":[\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards\"],"
                  "\"Severity\":\"Critical\","
                  "\"Resolution\":\"Provide a valid URI and resubmit the request.\""
                "}]"
              "}"
            "}",
            cdu_id, cdu_id, cdu_id);
        response->content_length = strlen(response->body);
        return SUCCESS; // 404 handled
    }

    // get control logic array size
    size_t control_logic_count = control_logic_manager_number_of_control_logics();

    size_t buffer_size = 2048 + (control_logic_count * 128);
    char *json = malloc(buffer_size);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "Failed to generate ControlLogics collection JSON");
        response->content_length = strlen(response->body);
        return ERROR_MEMORY;
    }

    int offset = snprintf(json, buffer_size,
        "{"
        "\"@odata.type\":\"#ControlLogicCollection.v1_0_0.ControlLogicCollection\"," 
        "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics\"," 
        "\"Name\":\"Kenmec ControlLogics Collection\"," 
        "\"Description\":\"Collection of ControlLogics for CDU %s\"," 
        "\"Members@odata.count\":%zu,"
        "\"Members\":[",
        cdu_id, cdu_id, control_logic_count);

    for (size_t i = 0; i < control_logic_count; ++i) {
        int n = snprintf(json + offset, buffer_size - (size_t)offset,
            "%s{\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics/%zu\"}",
            (i == 0 ? "" : ","), cdu_id, i + 1);
        if (n < 0) break;
        offset += n;
        if ((size_t)offset >= buffer_size - 4) break;
    }

    snprintf(json + offset, buffer_size - (size_t)offset, "]}" );

    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");
    strcpy(response->body, json);
    response->content_length = strlen(response->body);
    
    free(json);
    return SUCCESS;
}


int handle_cdu_oem_control_logics_member(const char *cdu_id, const char *member_id, http_response_t *response) {
    if (!cdu_id || !member_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    // Validate CDU existence - only CDU "1" exists in this demo
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body),
            "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%s was not found.\"}}",
            cdu_id, member_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Verify member ID maps to existing HID index
    uint16_t control_logic_count = control_logic_manager_number_of_control_logics();

    long idx = strtol(member_id, NULL, 10);
    if (idx <= 0 || (size_t)idx > control_logic_count) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body),
            "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%s was not found.\"}}",
            cdu_id, member_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Build a minimal IOBoard member payload
    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");
    snprintf(response->body, sizeof(response->body),
        "{"
          "\"@odata.type\":\"#ControlLogic.v1_0_0.ControlLogic\"," 
          "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics/%s\"," 
          "\"Id\":\"%s\"," 
          "\"Name\":\"Control Logic %s\"," 
          "\"Status\":{\"State\":\"Enabled\",\"Health\":\"OK\"}," 
          "\"Actions\":{"
            "\"Oem\":{"
              "\"#ControlLogic.Read\":{"
                "\"target\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics/%s/Actions/Oem/ControlLogic.Read\"," 
                "\"title\":\"Read Control Logic\""
              "},"
              "\"#ControlLogic.Write\":{"
                "\"target\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/ControlLogics/%s/Actions/Oem/ControlLogic.Write\"," 
                "\"title\":\"Write Control Logic\""
              "}"
            "}"
          "}"
        "}",
        cdu_id, member_id, member_id, member_id,
        cdu_id, member_id,
        cdu_id, member_id);
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_cdu_oem_ioboards(const char *cdu_id, http_response_t *response) {
    if (!cdu_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    // Validate CDU existence - only CDU "1" exists in this demo
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body),
            "{"
              "\"error\":{"
                "\"code\":\"Base.1.15.0.ResourceMissingAtURI\","
                "\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards was not found.\","
                "\"@Message.ExtendedInfo\":[{"
                  "\"@odata.type\":\"#Message.v1_1_1.Message\","
                  "\"MessageId\":\"Base.1.15.0.ResourceMissingAtURI\","
                  "\"Message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards was not found.\","
                  "\"MessageArgs\":[\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards\"],"
                  "\"Severity\":\"Critical\","
                  "\"Resolution\":\"Provide a valid URI and resubmit the request.\""
                "}]"
              "}"
            "}",
            cdu_id, cdu_id, cdu_id);
        response->content_length = strlen(response->body);
        return SUCCESS; // 404 handled
    }

    // Initialize HID bridge (idempotent for our simple stub)
    redfish_hid_init();

    // Query HID devices to determine IOBoards dynamically
    uint16_t hid_pid_list[32] = {0};
    size_t hid_count = 0;
    if (redfish_hid_device_list_get(hid_pid_list, 32, &hid_count) != SUCCESS) {
        hid_count = 0;
    }

    size_t buffer_size = 2048 + (hid_count * 128);
    char *json = malloc(buffer_size);
    if (!json) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "Failed to generate IOBoards collection JSON");
        response->content_length = strlen(response->body);
        return ERROR_MEMORY;
    }

    int offset = snprintf(json, buffer_size,
        "{"
        "\"@odata.type\":\"#KenmecIOBoardCollection.IOBoardCollection\"," 
        "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards\"," 
        "\"Name\":\"Kenmec IOBoards Collection\"," 
        "\"Description\":\"Collection of IO Boards for CDU %s\"," 
        "\"Members@odata.count\":%zu,"
        "\"Members\":[",
        cdu_id, cdu_id, hid_count);

    for (size_t i = 0; i < hid_count; ++i) {
        int n = snprintf(json + offset, buffer_size - (size_t)offset,
            "%s{\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%zu\"}",
            (i == 0 ? "" : ","), cdu_id, i + 1);
        if (n < 0) break;
        offset += n;
        if ((size_t)offset >= buffer_size - 4) break;
    }

    snprintf(json + offset, buffer_size - (size_t)offset, "]}" );

    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");
    strcpy(response->body, json);
    response->content_length = strlen(response->body);
    
    free(json);
    return SUCCESS;
}

int handle_cdu_oem_ioboard_member(const char *cdu_id, const char *member_id, http_response_t *response) {
    if (!cdu_id || !member_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    // Validate CDU existence - only CDU "1" exists in this demo
    if (strcmp(cdu_id, "1") != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body),
            "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%s was not found.\"}}",
            cdu_id, member_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Verify member ID maps to existing HID index
    uint16_t hid_pid_list[32] = {0};
    size_t hid_count = 0;
    if (redfish_hid_device_list_get(hid_pid_list, 32, &hid_count) != SUCCESS) {
        hid_count = 0;
    }

    long idx = strtol(member_id, NULL, 10);
    if (idx <= 0 || (size_t)idx > hid_count) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, "application/json");
        snprintf(response->body, sizeof(response->body),
            "{\"error\":{\"code\":\"Base.1.15.0.ResourceMissingAtURI\",\"message\":\"The resource at the URI /redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%s was not found.\"}}",
            cdu_id, member_id);
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Build a minimal IOBoard member payload
    response->status_code = HTTP_OK;
    strcpy(response->content_type, "application/json");
    snprintf(response->body, sizeof(response->body),
        "{"
          "\"@odata.type\":\"#KenmecIOBoard.v1_0_0.IOBoard\"," 
          "\"@odata.id\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%s\"," 
          "\"Id\":\"%s\"," 
          "\"Name\":\"IO Board %s\"," 
          "\"Status\":{\"State\":\"Enabled\",\"Health\":\"OK\"}," 
          "\"Actions\":{"
            "\"Oem\":{"
              "\"#KenmecIOBoard.Read\":{"
                "\"target\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%s/Actions/Oem/KenmecIOBoard.Read\"," 
                "\"title\":\"Read HID Board\""
              "},"
              "\"#KenmecIOBoard.Write\":{"
                "\"target\":\"/redfish/v1/ThermalEquipment/CDUs/%s/Oem/Kenmec/IOBoards/%s/Actions/Oem/KenmecIOBoard.Write\"," 
                "\"title\":\"Write HID Board\""
              "}"
            "}"
          "},"
          "\"Oem\":{\"Kenmec\":{\"Pid\":%u}}"
        "}",
        cdu_id, member_id, member_id, member_id,
        cdu_id, member_id,
        cdu_id, member_id,
        (unsigned)hid_pid_list[idx - 1]);
    response->content_length = strlen(response->body);
    return SUCCESS;
}

// DELETE handlers (remove resources)
int handle_chassis_delete(const char *chassis_id, http_response_t *response) {
    if (!chassis_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll return a success response
    // In a real implementation, you would actually remove the resource
    response->status_code = HTTP_NO_CONTENT;
    strcpy(response->body, "");
    response->content_length = 0;

    return SUCCESS;
}

int handle_system_delete(const char *system_id, http_response_t *response) {
    if (!system_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll return a success response
    response->status_code = HTTP_NO_CONTENT;
    strcpy(response->body, "");
    response->content_length = 0;

    return SUCCESS;
}

int handle_manager_delete(const char *manager_id, http_response_t *response) {
    if (!manager_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    // For demo purposes, we'll return a success response
    response->status_code = HTTP_NO_CONTENT;
    strcpy(response->body, "");
    response->content_length = 0;

    return SUCCESS;
}

int handle_certificate_service_replace_certificate(const http_request_t *request, http_response_t *response)
{
	if (!request || !response) return ERROR_INVALID_PARAM;

	cJSON *json = cJSON_Parse(request->body);
	if (!json) {
		response->status_code = HTTP_BAD_REQUEST;
		strcpy(response->content_type, CONTENT_TYPE_JSON);
		strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.MalformedJSON\",\"message\":\"The request body contains malformed JSON.\"}}\n");
		response->content_length = strlen(response->body);
		return SUCCESS;
	}

	cJSON *cert_uri = cJSON_GetObjectItem(json, "CertificateUri");
	cJSON *cert_str = cJSON_GetObjectItem(json, "CertificateString");
	cJSON *cert_type = cJSON_GetObjectItem(json, "CertificateType");
	const char *uri_id = NULL;
	if (cert_uri && cJSON_IsObject(cert_uri)) {
		cJSON *odata = cJSON_GetObjectItem(cert_uri, "@odata.id");
		if (odata && cJSON_IsString(odata)) uri_id = cJSON_GetStringValue(odata);
	}
	if (!cert_str || !cJSON_IsString(cert_str) || !cert_type || !cJSON_IsString(cert_type) || !uri_id) {
		cJSON_Delete(json);
		response->status_code = HTTP_BAD_REQUEST;
		strcpy(response->content_type, CONTENT_TYPE_JSON);
		strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.PropertyMissing\",\"message\":\"CertificateUri.@odata.id, CertificateString, and CertificateType are required.\"}}\n");
		response->content_length = strlen(response->body);
		return SUCCESS;
	}

	const char *pem = cJSON_GetStringValue(cert_str);
	const char *ctype = cJSON_GetStringValue(cert_type);
	printf("ReplaceCertificate called for %s, type=%s, cert-bytes=%zu\n", uri_id, ctype, pem ? strlen(pem) : 0);

    system_certificate_type_t replace_cert_type = SYSTEM_CERTIFICATE_TYPE_UNKNOWN;

    // Determine the type of certificate, CertificateUri can be:
    // - "NetworkProtocol/HTTPS/Certificates/1"
    // - "SecurityPolicy/TLS/Server/TrustedCertificates/1"
    if (strstr(uri_id, "NetworkProtocol/HTTPS/Certificates/1") != NULL) {
        replace_cert_type = SYSTEM_CERTIFICATE_TYPE_SERVER_CERTIFICATE;
    } else if (strstr(uri_id, "SecurityPolicy/TLS/Server/TrustedCertificates/1") != NULL) {
        replace_cert_type = SYSTEM_CERTIFICATE_TYPE_ROOT_CERTIFICATE;
    }


    if (replace_cert_type == SYSTEM_CERTIFICATE_TYPE_UNKNOWN) {
        cJSON_Delete(json);
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.PropertyMissing\",\"message\":\"CertificateUri.@odata.id, CertificateString, and CertificateType are required.\"}}\n");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
    

    switch (replace_cert_type) {
        case SYSTEM_CERTIFICATE_TYPE_SERVER_CERTIFICATE:
            // Store the certificate to the system certificate store
            if (system_certificate_store_pem(pem) != 0) {
                cJSON_Delete(json);
                response->status_code = HTTP_INTERNAL_SERVER_ERROR;
                strcpy(response->content_type, CONTENT_TYPE_JSON);
                strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"Failed to store certificate.\"}}\n");
                response->content_length = strlen(response->body);
                return SUCCESS;
            }
            
            // // Load the certificate from the system certificate store and print it
            // char pem_out[4096];
            // if (system_certificate_load_pem(pem_out, sizeof(pem_out)) == 0) {
            //     printf("Certificate length: %zu\n", strlen(pem_out));
            //     printf("Certificate: %s\n", pem_out);
            // }

            response->status_code = HTTP_OK;
            strcpy(response->content_type, CONTENT_TYPE_JSON);
            snprintf(response->body, sizeof(response->body),
                "{\n"
                "  \"@odata.type\": \"#CertificateService.v1_1_0.ReplaceCertificateResponse\",\n"
                "  \"Certificate\": {\n"
                "    \"@odata.id\": \"/redfish/v1/Managers/Kenmec/NetworkProtocol/HTTPS/Certificates/1\"\n"
                "  }\n"
                "}\n");
            response->content_length = strlen(response->body);
            break;
        case SYSTEM_CERTIFICATE_TYPE_ROOT_CERTIFICATE:
            // Store the certificate to the system root certificate store
            if (system_root_certificate_store_pem(pem) != 0) {
                cJSON_Delete(json);
                response->status_code = HTTP_INTERNAL_SERVER_ERROR;
                strcpy(response->content_type, CONTENT_TYPE_JSON);
                strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"Failed to store certificate.\"}}\n");
                response->content_length = strlen(response->body);
                return SUCCESS;
            }

            response->status_code = HTTP_OK;
            strcpy(response->content_type, CONTENT_TYPE_JSON);
            snprintf(response->body, sizeof(response->body),
                "{\n"
                "  \"@odata.type\": \"#CertificateService.v1_1_0.ReplaceCertificateResponse\",\n"
                "  \"@Redfish.SettingsApplyTime\": { \"ApplyTime\": \"OnReset\" },\n"
                "  \"Certificate\": {\n"
                "    \"@odata.id\": \"/redfish/v1/Managers/Kenmec/SecurityPolicy/TLS/Server/TrustedCertificates/1\"\n"
                "  }\n"
                "}\n");
            response->content_length = strlen(response->body);
            break;
        default:
            cJSON_Delete(json);
            response->status_code = HTTP_BAD_REQUEST;
            strcpy(response->content_type, CONTENT_TYPE_JSON);
            strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.PropertyMissing\",\"message\":\"CertificateUri.@odata.id, CertificateString, and CertificateType are required.\"}}\n");
            response->content_length = strlen(response->body);
            break;
    }

	cJSON_Delete(json);
	return SUCCESS;
}

int handle_manager_security_policy_trusted_certificates(http_response_t *response)
{
    if (!response) return ERROR_INVALID_PARAM;

    // Load root certificate from database
    char root_cert_pem[8192];
    int cert_len = system_root_certificate_load_pem(root_cert_pem, sizeof(root_cert_pem));
    
    if (cert_len != 0) {
        // No root certificate available, return empty collection
        snprintf(response->body, sizeof(response->body),
            "{\n"
            "  \"@odata.type\": \"#CertificateCollection.CertificateCollection\",\n"
            "  \"@odata.id\": \"/redfish/v1/Managers/%s/SecurityPolicy/TLS/Server/TrustedCertificates\",\n"
            "  \"Name\": \"Trusted Certificates Collection\",\n"
            "  \"Description\": \"Collection of trusted certificates for %s Manager\",\n"
            "  \"Members@odata.count\": 0,\n"
            "  \"Members\": []\n"
            "}",
            MANAGER_ID_KENMEC, MANAGER_ID_KENMEC);
    } else {
        // Root certificate available, return collection with one member
        snprintf(response->body, sizeof(response->body),
            "{\n"
            "  \"@odata.type\": \"#CertificateCollection.CertificateCollection\",\n"
            "  \"@odata.id\": \"/redfish/v1/Managers/%s/SecurityPolicy/TLS/Server/TrustedCertificates\",\n"
            "  \"Name\": \"Trusted Certificates Collection\",\n"
            "  \"Description\": \"Collection of trusted certificates for %s Manager\",\n"
            "  \"Members@odata.count\": 1,\n"
            "  \"Members\": [\n"
            "    {\n"
            "      \"@odata.id\": \"/redfish/v1/Managers/%s/SecurityPolicy/TLS/Server/TrustedCertificates/1\"\n"
            "    }\n"
            "  ]\n"
            "}",
            MANAGER_ID_KENMEC, MANAGER_ID_KENMEC, MANAGER_ID_KENMEC);
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_manager_security_policy_trusted_certificate(const char *cert_id, http_response_t *response)
{
    if (!cert_id || !response) return ERROR_INVALID_PARAM;

    // Load root certificate from database
    char root_cert_pem[8192];
    int cert_len = system_root_certificate_load_pem(root_cert_pem, sizeof(root_cert_pem));
    
    if (cert_len != 0) {
        // No root certificate available, return 404
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.ResourceNotFound\",\"message\":\"The requested resource was not found.\"}}\n");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Parse certificate information
    char issuer_country[64] = "Unknown";
    char issuer_state[64] = "Unknown";
    char issuer_city[64] = "Unknown";
    char issuer_org[64] = "Unknown";
    char issuer_ou[64] = "Unknown";
    char issuer_cn[64] = "Unknown";
    char subject_country[64] = "Unknown";
    char subject_state[64] = "Unknown";
    char subject_city[64] = "Unknown";
    char subject_org[64] = "Unknown";
    char subject_ou[64] = "Unknown";
    char subject_cn[64] = "Unknown";
    char valid_not_before[32] = "Unknown";
    char valid_not_after[32] = "Unknown";
    
    // Parse certificate using redfish_parse_certificate_info
    redfish_parse_certificate_info(root_cert_pem, 
        issuer_country, sizeof(issuer_country),
        issuer_state, sizeof(issuer_state),
        issuer_city, sizeof(issuer_city),
        issuer_org, sizeof(issuer_org),
        issuer_ou, sizeof(issuer_ou),
        issuer_cn, sizeof(issuer_cn),
        subject_country, sizeof(subject_country),
        subject_state, sizeof(subject_state),
        subject_city, sizeof(subject_city),
        subject_org, sizeof(subject_org),
        subject_ou, sizeof(subject_ou),
        subject_cn, sizeof(subject_cn),
        valid_not_before, sizeof(valid_not_before),
        valid_not_after, sizeof(valid_not_after));

    // Escape the PEM certificate for JSON output
    char escaped_pem[16384];
    redfish_escape_pem_for_json(root_cert_pem, escaped_pem, sizeof(escaped_pem));

    // Return certificate details
    snprintf(response->body, sizeof(response->body),
        "{\n"
        "  \"@odata.type\": \"#Certificate.v1_4_0.Certificate\",\n"
        "  \"@odata.id\": \"/redfish/v1/Managers/%s/SecurityPolicy/TLS/Server/TrustedCertificates/%s\",\n"
        "  \"Id\": \"%s\",\n"
        "  \"Name\": \"Trusted Certificate\",\n"
        "  \"Description\": \"Trusted certificate for %s Manager\",\n"
        "  \"CertificateString\": \"%s\",\n"
        "  \"CertificateType\": \"PEM\",\n"
        "  \"Issuer\": {\n"
        "    \"Country\": \"%s\",\n"
        "    \"State\": \"%s\",\n"
        "    \"City\": \"%s\",\n"
        "    \"Organization\": \"%s\",\n"
        "    \"OrganizationalUnit\": \"%s\",\n"
        "    \"CommonName\": \"%s\"\n"
        "  },\n"
        "  \"Subject\": {\n"
        "    \"Country\": \"%s\",\n"
        "    \"State\": \"%s\",\n"
        "    \"City\": \"%s\",\n"
        "    \"Organization\": \"%s\",\n"
        "    \"OrganizationalUnit\": \"%s\",\n"
        "    \"CommonName\": \"%s\"\n"
        "  },\n"
        "  \"ValidNotBefore\": \"%s\",\n"
        "  \"ValidNotAfter\": \"%s\"\n"
        "}",
        MANAGER_ID_KENMEC, cert_id, cert_id, MANAGER_ID_KENMEC, escaped_pem,
        issuer_country, issuer_state, issuer_city, issuer_org, issuer_ou, issuer_cn,
        subject_country, subject_state, subject_city, subject_org, subject_ou, subject_cn,
        valid_not_before, valid_not_after);

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_manager_network_protocol(const char *manager_id, http_response_t *response)
{
    if (!manager_id || !response) {
        return ERROR_INVALID_PARAM;
    }

    snprintf(response->body, sizeof(response->body),
        "{\n"
        "  \"@odata.type\": \"#ManagerNetworkProtocol.v1_12_0.ManagerNetworkProtocol\",\n"
        "  \"@odata.id\": \"/redfish/v1/Managers/%s/NetworkProtocol\",\n"
        "  \"Id\": \"NetworkProtocol\",\n"
        "  \"Name\": \"Manager Network Protocol\",\n"
        "  \"HTTP\": { \"ProtocolEnabled\": true, \"Port\": %d },\n"
        "  \"HTTPS\": { \"ProtocolEnabled\": true, \"Port\": %d, \"Certificates\": { \"@odata.id\": \"/redfish/v1/Managers/%s/NetworkProtocol/HTTPS/Certificates\" } }\n"
        "}\n",
        manager_id, DEFAULT_HTTP_PORT, DEFAULT_PORT, manager_id);

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_manager_network_protocol_https_certificates(const char *manager_id, http_response_t *response)
{
    if (!manager_id || !response) return ERROR_INVALID_PARAM;

    // Load server certificate from database/store
    char server_cert_pem[8192];
    int rc = system_certificate_load_pem(server_cert_pem, sizeof(server_cert_pem));

    if (rc != 0) {
        // No server certificate available, return empty collection
        snprintf(response->body, sizeof(response->body),
            "{\n"
            "  \"@odata.type\": \"#CertificateCollection.CertificateCollection\",\n"
            "  \"@odata.id\": \"/redfish/v1/Managers/%s/NetworkProtocol/HTTPS/Certificates\",\n"
            "  \"Name\": \"HTTPS Certificates Collection\",\n"
            "  \"Description\": \"Collection of HTTPS server certificates for %s Manager\",\n"
            "  \"Members@odata.count\": 0,\n"
            "  \"Members\": []\n"
            "}",
            manager_id, manager_id);
    } else {
        // Certificate available, return collection with one member
        snprintf(response->body, sizeof(response->body),
            "{\n"
            "  \"@odata.type\": \"#CertificateCollection.CertificateCollection\",\n"
            "  \"@odata.id\": \"/redfish/v1/Managers/%s/NetworkProtocol/HTTPS/Certificates\",\n"
            "  \"Name\": \"HTTPS Certificates Collection\",\n"
            "  \"Description\": \"Collection of HTTPS server certificates for %s Manager\",\n"
            "  \"Members@odata.count\": 1,\n"
            "  \"Members\": [\n"
            "    {\n"
            "      \"@odata.id\": \"/redfish/v1/Managers/%s/NetworkProtocol/HTTPS/Certificates/1\"\n"
            "    }\n"
            "  ]\n"
            "}",
            manager_id, manager_id, manager_id);
    }

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_manager_network_protocol_https_certificate_member(const char *manager_id, const char *cert_id, http_response_t *response)
{
    if (!manager_id || !cert_id || !response) return ERROR_INVALID_PARAM;

    // Load server certificate from database/store
    char server_cert_pem[8192];
    int rc = system_certificate_load_pem(server_cert_pem, sizeof(server_cert_pem));
    if (rc != 0) {
        response->status_code = HTTP_NOT_FOUND;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.ResourceNotFound\",\"message\":\"The requested resource was not found.\"}}\n");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Escape PEM for JSON
    char escaped_pem[16384];
    redfish_escape_pem_for_json(server_cert_pem, escaped_pem, sizeof(escaped_pem));

    snprintf(response->body, sizeof(response->body),
        "{\n"
        "  \"@odata.type\": \"#Certificate.v1_4_0.Certificate\",\n"
        "  \"@odata.id\": \"/redfish/v1/Managers/%s/NetworkProtocol/HTTPS/Certificates/%s\",\n"
        "  \"Id\": \"%s\",\n"
        "  \"Name\": \"HTTPS Server Certificate\",\n"
        "  \"Description\": \"HTTPS server certificate for %s Manager\",\n"
        "  \"CertificateString\": \"%s\",\n"
        "  \"CertificateType\": \"PEM\"\n"
        "}",
        manager_id, cert_id, cert_id, manager_id, escaped_pem);

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    response->content_length = strlen(response->body);
    return SUCCESS;
}

int handle_update_service(http_response_t *response)
{
    if (!response) return ERROR_INVALID_PARAM;

    snprintf(response->body, sizeof(response->body),
        "{\n"
        "  \"@odata.type\": \"#UpdateService.v1_15_0.UpdateService\",\n"
        "  \"@odata.id\": \"/redfish/v1/UpdateService\",\n"
        "  \"Id\": \"UpdateService\",\n"
        "  \"Name\": \"Update Service\",\n"
        "  \"MultipartHttpPushUri\": \"/UpdateFirmwareMultipart\"\n"
        "}\n");

    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    response->content_length = strlen(response->body);
    return SUCCESS;
}

// Binary-safe memory search function
static void* binary_search(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len) {
    if (needle_len == 0 || haystack_len < needle_len) return NULL;
    
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(h + i, n, needle_len) == 0) {
            return (void *)(h + i);
        }
    }
    return NULL;
}

// Extract boundary from Content-Type header
static char* extract_boundary(const char *content_type) {
    const char *boundary_start = strstr(content_type, "boundary=");
    if (!boundary_start) return NULL;
    
    boundary_start += 9; // Skip "boundary="
    
    // Find end of boundary (space, semicolon, or end of string)
    const char *boundary_end = boundary_start;
    while (*boundary_end && *boundary_end != ' ' && *boundary_end != ';' && *boundary_end != '\r' && *boundary_end != '\n') {
        boundary_end++;
    }
    
    size_t len = (size_t)(boundary_end - boundary_start);
    char *boundary = malloc(len + 1);
    if (!boundary) return NULL;
    
    strncpy(boundary, boundary_start, len);
    boundary[len] = '\0';
    return boundary;
}

// Parse multipart data and extract file content
static int parse_multipart_file(const char *filepath, const char *boundary, const char *output_filepath, long *extracted_bytes) {
    FILE *input = fopen(filepath, "rb");
    if (!input) return -1;
    
    // Read the entire file into memory
    fseek(input, 0, SEEK_END);
    long file_size = ftell(input);
    fseek(input, 0, SEEK_SET);
    
    char *file_data = malloc(file_size + 1);
    if (!file_data) {
        fclose(input);
        return -1;
    }
    
    if (fread(file_data, 1, file_size, input) != (size_t)file_size) {
        free(file_data);
        fclose(input);
        return -1;
    }
    file_data[file_size] = '\0';  // Null terminate for string operations
    fclose(input);
    
    // RFC 2046 multipart boundary format tokens
    char boundary_token[256];           // --boundary
    char next_boundary_crlf[258];       // \r\n--boundary
    char end_boundary_token[258];       // --boundary--
    snprintf(boundary_token, sizeof(boundary_token), "--%s", boundary);
    snprintf(next_boundary_crlf, sizeof(next_boundary_crlf), "\r\n--%s", boundary);
    snprintf(end_boundary_token, sizeof(end_boundary_token), "--%s--", boundary);

    // Iterate parts; extract the one with name="UpdateFile" (or fallback to name="file")
    char *cursor = file_data;
    char *found_content_start = NULL;
    long found_content_len = 0;

    while (1) {
        size_t remaining_all = file_size - (cursor - file_data);
        char *bstart = binary_search(cursor, remaining_all, boundary_token, strlen(boundary_token));
        if (!bstart) break;

        // Advance past boundary and line break
        char *p = bstart + strlen(boundary_token);
        if (*p == '\r') p++;
        if (*p == '\n') p++;

        // Locate headers end
        size_t remain = file_size - (p - file_data);
        char *hdr_end = binary_search(p, remain, "\r\n\r\n", 4);
        if (hdr_end) {
            hdr_end += 4;
        } else {
            hdr_end = binary_search(p, remain, "\n\n", 2);
            if (!hdr_end) break; // malformed
            hdr_end += 2;
        }

        // Parse headers to find Content-Disposition name
        size_t hdr_len = (size_t)(hdr_end - p);
        const char *cd = (const char *)p;
        const char *cd_end = (const char *)p + hdr_len;
        int is_update_file = 0;
        while (cd < cd_end) {
            const char *line_end = memchr(cd, '\n', (size_t)(cd_end - cd));
            if (!line_end) line_end = cd_end;
            if ((size_t)(line_end - cd) >= 19 && strncasecmp(cd, "Content-Disposition:", 19) == 0) {
                // look for name="UpdateFile" or name="file"
                if (strstr(cd, "name=\"UpdateFile\"") || strstr(cd, "name=\"file\"")) {
                    is_update_file = 1;
                }
            }
            cd = line_end + 1;
        }

        // Content starts at hdr_end
        char *content_start = hdr_end;

        // Find next boundary (start of next part) or final boundary using full-line matches
        size_t rem_after = file_size - (content_start - file_data);
        char next_full[260];      // "\r\n--boundary\r\n"
        char next_nl[260];        // "\n--boundary\r\n" (tolerate lone LF)
        char end_full[260];       // "\r\n--boundary--"
        char end_nl[260];         // "\n--boundary--"
        snprintf(next_full, sizeof(next_full), "\r\n--%s\r\n", boundary);
        snprintf(next_nl, sizeof(next_nl),   "\n--%s\r\n",  boundary);
        snprintf(end_full, sizeof(end_full),  "\r\n--%s--",  boundary);
        snprintf(end_nl, sizeof(end_nl),      "\n--%s--",     boundary);

        char *nb1 = binary_search(content_start, rem_after, next_full, strlen(next_full));
        char *nb2 = binary_search(content_start, rem_after, next_nl,   strlen(next_nl));
        char *endb1 = binary_search(content_start, rem_after, end_full, strlen(end_full));
        char *endb2 = binary_search(content_start, rem_after, end_nl,   strlen(end_nl));

        // Pick earliest valid candidates
        char *nb = NULL; if (nb1 && (!nb || nb1 < nb)) nb = nb1; if (nb2 && (!nb || nb2 < nb)) nb = nb2;
        char *endb = NULL; if (endb1 && (!endb || endb1 < endb)) endb = endb1; if (endb2 && (!endb || endb2 < endb)) endb = endb2;

        char *content_end = NULL;
        int used_next = 0;
        if (nb && endb) {
            if (nb < endb) { content_end = nb; used_next = 1; } else { content_end = endb; used_next = 0; }
        } else if (nb) {
            content_end = nb; used_next = 1;
        } else if (endb) {
            content_end = endb; used_next = 0;
        } else {
            // No valid boundary line found; abort this part
            break;
        }

        if (is_update_file) {
            found_content_start = content_start;
            found_content_len = (long)(content_end - content_start);
            break;
        }

        // Advance cursor to search for next part
        if (used_next) {
            cursor = content_end + strlen(next_full);
        } else {
            // Final boundary reached; stop iterating
            break;
        }
    }

    if (!found_content_start || found_content_len <= 0) {
        free(file_data);
        return -1;
    }

    // Write extracted content to output file
    FILE *output = fopen(output_filepath, "wb");
    if (!output) {
        free(file_data);
        return -1;
    }
    if (fwrite(found_content_start, 1, (size_t)found_content_len, output) != (size_t)found_content_len) {
        free(file_data);
        fclose(output);
        return -1;
    }
    *extracted_bytes = found_content_len;

    free(file_data);
    fclose(output);
    return 0;
}

int handle_update_service_multipart_upload(const http_request_t *request, http_response_t *response)
{
    if (!request || !response) return ERROR_INVALID_PARAM;

    // Validate Content-Type multipart
    const char *content_type = NULL;
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i][0], "Content-Type") == 0) {
            content_type = request->headers[i][1];
            break;
        }
    }

    if (!content_type || strstr(content_type, "multipart/") == NULL) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.MalformedJSON\",\"message\":\"Content-Type must be multipart.*\"}}\n");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Extract boundary from Content-Type
    char *boundary = extract_boundary(content_type);
    if (!boundary) {
        response->status_code = HTTP_BAD_REQUEST;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.MalformedJSON\",\"message\":\"Missing boundary in multipart Content-Type\"}}\n");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Create output file path for extracted content
    char extracted_filepath[256];
    snprintf(extracted_filepath, sizeof(extracted_filepath), "%s.extracted", request->upload_tmp_path);
    
    // Parse multipart and extract file content
    long extracted_bytes = 0;
    int parse_result = parse_multipart_file(request->upload_tmp_path, boundary, extracted_filepath, &extracted_bytes);
    
    free(boundary);
    
    if (parse_result != 0) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"Failed to parse multipart data\"}}\n");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }
    
    // Replace original file with extracted content
    if (rename(extracted_filepath, request->upload_tmp_path) != 0) {
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, CONTENT_TYPE_JSON);
        strcpy(response->body, "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"Failed to save extracted file\"}}\n");
        response->content_length = strlen(response->body);
        return SUCCESS;
    }

    // Respond with path where clean data was stored
    response->status_code = HTTP_OK;
    strcpy(response->content_type, CONTENT_TYPE_JSON);
    snprintf(response->body, sizeof(response->body),
        "{\n"
        "  \"Message\": \"Multipart upload accepted and file extracted\",\n"
        "  \"SavedTo\": \"%s\",\n"
        "  \"Bytes\": %ld\n"
        "}\n",
        request->upload_tmp_path[0] ? request->upload_tmp_path : "",
        extracted_bytes);
    response->content_length = strlen(response->body);

    // Trigger firmware update
    system_firmware_update();
    
    return SUCCESS;
}
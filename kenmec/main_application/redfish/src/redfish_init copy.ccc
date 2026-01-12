#include "dexatek/main_application/include/application_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>

#include "config.h"
#include "tls_server.h"
#include "redfish_client_info_handle.h"
#include "redfish_server.h"
#include "redfish_hid_bridge.h"
#include "dexatek/main_application/include/application_common.h"
#include "dexatek/main_application/include/utilities/net_utilities.h"
#include "dexatek/main_application/services/include/mdns_service.h"

#include "kenmec/main_application/kenmec_config.h"

#include "ethernet.h"
    
// #define DEFAULT_PORT 8443
// #define DEFAULT_HTTP_PORT 8080
// #define BUFFER_SIZE 40960

static const char* tag = "redfish";

static MutexContext _mutex_handle = NULL;

static BOOL _thread_aborted = FALSE;
static PlatformTaskHandle _thread_handle = NULL;
static char _current_carrier = 0;

// Global TLS server context
static tls_server_context_t g_tls_ctx;

// mDNS service config
static MdnsServiceConfig _mdns_http_config = {0};
static MdnsServiceConfig _mdns_https_config = {0};

void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -p <port>     HTTPS port number (default: %d)\n", DEFAULT_PORT);
    printf("  -h <port>     HTTP port number (default: %d)\n", DEFAULT_HTTP_PORT);
    printf("  -c <cert>     Certificate file (default: %s)\n", DEFAULT_CERT_FILE);
    printf("  -k <key>      Private key file (default: %s)\n", DEFAULT_KEY_FILE);
    printf("  --help        Show this help message\n");
}

// Client connection handler
int handle_client_connection(int client_fd) {
    char request_buffer[BUFFER_SIZE];
    char response_buffer[BUFFER_SIZE];
    http_request_t request;
    http_response_t response;
    
    // Initialize buffers to zero
    memset(request_buffer, 0, sizeof(request_buffer));
    memset(response_buffer, 0, sizeof(response_buffer));
    
    // Create a new SSL context for this client
    mbedtls_ssl_context ssl;
    mbedtls_ssl_init(&ssl);
    
    // Create persistent mbedtls_net_context for this connection
    mbedtls_net_context client_net_ctx;
    mbedtls_net_init(&client_net_ctx);
    client_net_ctx.fd = client_fd;

    printf("[%s] Handling client connection on fd %d\n", __FUNCTION__, client_fd);
    
    int ret = tls_server_establish_ssl(&g_tls_ctx, client_fd, &ssl, &client_net_ctx);
    if (ret != SUCCESS) {
        error(tag, "Failed to establish SSL connection");
        mbedtls_ssl_free(&ssl);
        close(client_fd);
        return ret;
    }
    
    // Read request from client
    int bytes_read = tls_server_read(&ssl, request_buffer, sizeof(request_buffer));
    if (bytes_read <= 0) {
        error(tag, "Failed to read request from client");
        tls_server_close_client(&ssl, client_fd);
        return ERROR_NETWORK;
    }

    printf("Received request %d: %d\n%s\n", bytes_read, strlen(request_buffer), request_buffer);

    // Parse HTTP request
    ret = parse_http_request(request_buffer, &request, 1);  // HTTPS
    if (ret != SUCCESS) {
        error(tag, "Failed to parse HTTP request");
        tls_server_close_client(&ssl, client_fd);
        return ret;
    }

    // Ensure full body is read based on Content-Length (HTTPS/TLS) with
    // streaming to file for large multipart uploads
    int expected_len = 0;
    for (int i = 0; i < request.header_count; i++) {
        if (strcasecmp(request.headers[i][0], "Content-Length") == 0) {
            expected_len = atoi(request.headers[i][1]);
            break;
        }
    }

    int is_upload = 0;
    if (strcmp(request.method, HTTP_METHOD_POST) == 0 && strstr(request.path, "/UpdateFirmwareMultipart") != NULL) {
        is_upload = 1;
    }

    // If client sent Expect: 100-continue, acknowledge before reading body
    if (is_upload) {
        for (int i = 0; i < request.header_count; i++) {
            if (strcasecmp(request.headers[i][0], "Expect") == 0 &&
                strncasecmp(request.headers[i][1], "100-continue", 12) == 0) {
                const char *cont = "HTTP/1.1 100 Continue\r\n\r\n";
                (void)tls_server_write(&ssl, cont, (int)strlen(cont));
                break;
            }
        }
    }

    if (!is_upload) {
        if (expected_len > request.content_length && expected_len < (int)sizeof(request.body)) {
            int remaining = expected_len - request.content_length;
            int offset = request.content_length;
            while (remaining > 0) {
                int n = tls_server_read(&ssl, request.body + offset, (size_t)remaining);
                if (n == MBEDTLS_ERR_SSL_WANT_READ) {
                    continue;
                }
                if (n <= 0) {
                    tls_server_close_client(&ssl, client_fd);
                    return ERROR_NETWORK;
                }
                offset += n;
                remaining -= n;
            }
            request.body[expected_len] = '\0';
            request.content_length = expected_len;
        }
    } else {
        // Stream multipart body to fixed firmware file path
        system_firmware_file_path(request.upload_tmp_path);
        int tmp_fd = open(request.upload_tmp_path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (tmp_fd < 0) {
            printf("Failed to create temp upload file (HTTPS)\n");
            tls_server_close_client(&ssl, client_fd);
            return ERROR_NETWORK;
        }
        // Write buffered bytes already read into request.body
        if (request.content_length > 0) {
            if (write(tmp_fd, request.body, (size_t)request.content_length) != request.content_length) {
                printf("Failed to write initial upload bytes (HTTPS)\n");
                close(tmp_fd);
                tls_server_close_client(&ssl, client_fd);
                return ERROR_NETWORK;
            }
        }
        int remaining = expected_len - request.content_length;
        unsigned char chunk[8192];
        int idle_retries = 0;
        const int max_idle_retries = 30; // ~30 iterations
        while (remaining > 0) {
            size_t to_read = (size_t)(remaining > (int)sizeof(chunk) ? (int)sizeof(chunk) : remaining);
            int n = tls_server_read(&ssl, (char *)chunk, to_read);
            if (n == MBEDTLS_ERR_SSL_WANT_READ) {
                if (++idle_retries > max_idle_retries) {
                    printf("tls read timeout exceeded; aborting upload (HTTPS)\n");
                    close(tmp_fd);
                    tls_server_close_client(&ssl, client_fd);
                    return ERROR_NETWORK;
                }
                continue;
            }
            if (n <= 0) {
                printf("Failed to read remaining upload bytes (HTTPS)\n");
                close(tmp_fd);
                tls_server_close_client(&ssl, client_fd);
                return ERROR_NETWORK;
            }
            idle_retries = 0;
            if (write(tmp_fd, chunk, (size_t)n) != n) {
                printf("Failed to write upload chunk (HTTPS)\n");
                close(tmp_fd);
                tls_server_close_client(&ssl, client_fd);
                return ERROR_NETWORK;
            }
            remaining -= n;
        }
        fsync(tmp_fd);
        close(tmp_fd);
    }

    printf("************\n");
    printf("Method: %s\n", request.method);
    printf("Path: %s\n", request.path);
    for (int i = 0; i < request.header_count; i++) {
        debug(tag, "Header: [%s] = [%s]", request.headers[i][0], request.headers[i][1]);
    }
    debug(tag, "Body: %s", request.body);
    debug(tag, "************");

    // Process Redfish request
    ret = process_redfish_request(&request, &response);

    
    if (ret != SUCCESS) {
        error(tag, "Failed to process Redfish request");
        tls_server_close_client(&ssl, client_fd);
        return ret;
    }

    // Generate HTTP response
    generate_http_response(&response, response_buffer, sizeof(response_buffer));

    debug(tag, "Sending response:\n%s\n", response_buffer);

    // Send response to client
    int bytes_written = tls_server_write(&ssl, response_buffer, strlen(response_buffer));
    if (bytes_written < 0) {
        error(tag, "Failed to write response to client");
        tls_server_close_client(&ssl, client_fd);
        return ERROR_NETWORK;
    }

    // Close client connection and cleanup
    tls_server_close_client(&ssl, client_fd);

    // Execute post action if specified
    if (response.post_action != LABEL_POST_ACTION_NONE) {
        printf("Executing post action %d after response sent\n", response.post_action);
        redfish_server_post_action(response.post_action, "HTTPS client");
    }

    return SUCCESS;
}


static TaskReturn _redfish_manager_process(void *param) 
{
    (void)param;
    
    // Prevent process termination on write() to closed sockets
    // This can happen when clients abort TLS/HTTP early; ignoring SIGPIPE ensures
    // we get an error return instead of a process crash.
    signal(SIGPIPE, SIG_IGN);

    int https_port = DEFAULT_PORT;
    int http_port = DEFAULT_HTTP_PORT;
    const char *cert_file = DEFAULT_CERT_FILE;
    const char *key_file = DEFAULT_KEY_FILE;
    const char *client_ca_file = CLIENT_CA_FILE;
    
    debug(tag, "RedfishDemo Server Starting...");
    debug(tag, "HTTPS Port: %d", https_port);
    debug(tag, "HTTP Port: %d", http_port);
   
#if 0
    // Wait for IPv4 to be available before starting the server
    printf("Waiting for IPv4 address to be available on eth0...\n");
    while (!is_ipv4_available()) {
        sleep(2);
        
        // Check if thread should be aborted
        if (_thread_aborted) {
            printf("Thread aborted while waiting for IPv4\n");
            pthread_exit(NULL);
        }
    }
    printf("IPv4 address available, proceeding with server initialization...\n");
#endif
    
    // Initialize HID bridge
    if (redfish_hid_init() != SUCCESS) {
        error(tag, "Failed to initialize HID bridge");
        pthread_exit(NULL);
    }

    // Initialize Redfish server
    if (redfish_server_init() != SUCCESS) {
        error(tag, "Failed to initialize Redfish server");
        pthread_exit(NULL);
    }
    
    // Initialize HTTP server
    int http_server_fd = -1;
    if (SUPPORT_HTTP) {
        if (http_server_init(http_port) != SUCCESS) {
            error(tag, "Failed to initialize HTTP server");
        } else {
            http_server_fd = http_server_get_fd();
            debug(tag, "HTTP server listening on port %d", http_port);
        }
    }
    
    // Initialize HTTPS server
    int https_server_fd = -1;
    if (SUPPORT_HTTPS) {
        if (tls_server_init(&g_tls_ctx, cert_file, key_file, client_ca_file, https_port) != SUCCESS) {
            error(tag, "Failed to initialize HTTPS server");
        } else {
            https_server_fd = g_tls_ctx.server_fd;
            debug(tag, "HTTPS server listening on port %d", https_port);
        }
    }
    
    debug(tag, "Server initialized successfully. Waiting for connections...");
    debug(tag, "Press Ctrl+C to stop the server");

    db_init();

    // Main server loop using select() for multiplexing
    while (1) {
        if (_thread_aborted) {
            printf("Thread aborted\n");
            break;
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);
        
        int max_fd = -1;
        
        // Add HTTP server to select set
        if (http_server_fd >= 0) {
            FD_SET(http_server_fd, &read_fds);
            if (http_server_fd > max_fd) max_fd = http_server_fd;
        }
        
        // Add HTTPS server to select set
        if (https_server_fd >= 0) {
            FD_SET(https_server_fd, &read_fds);
            if (https_server_fd > max_fd) max_fd = https_server_fd;
        }
        
        if (max_fd < 0) {
            printf("No servers available max_fd: %d\n", max_fd);
            break;
        }
        
        // Wait for activity on any server socket
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            error(tag, "Select error");
            continue;
        }
        
        // Check HTTP server for new connections
        if (http_server_fd >= 0 && FD_ISSET(http_server_fd, &read_fds)) {
            int client_fd = accept(http_server_fd, NULL, NULL);
            if (client_fd >= 0) {
                debug(tag, "New HTTP connection accepted");
                handle_http_client_connection(client_fd);
            }
        }
        
        // Check HTTPS server for new connections
        if (https_server_fd >= 0 && FD_ISSET(https_server_fd, &read_fds)) {
            int client_fd;
            int ret = tls_server_accept_client(&g_tls_ctx, &client_fd);
            if (ret == SUCCESS && client_fd >= 0) {
                debug(tag, "New HTTPS connection accepted");
                handle_https_client_connection(client_fd);
            }
        }
    }
    
    // Cleanup
    if (http_server_fd >= 0) {
        close(http_server_fd);
    }
    if (https_server_fd >= 0) {
        close(https_server_fd);
    }
    tls_server_cleanup(&g_tls_ctx);
    http_server_cleanup();

    pthread_exit(NULL);
}


int redfish_init(void)
{
	// debug(tag, "[%s] +", __FUNCTION__);
	int ret = SUCCESS;

	_thread_aborted = FALSE;
	_current_carrier = 0;

	// Init a mutex for the access of Ethernet component.
	if(_mutex_handle != NULL) {
		mutex_delete(_mutex_handle);
		_mutex_handle = NULL;
	}

	_mutex_handle = mutex_create();

#if CONFIG_MDNS_ENABLE
    /* mDNS service init for HTTP */
    // Get the latest 3 bytes of the eth0 MAC address and concat to the MDNS name.
    uint8_t mac_address[6];
    net_mac_get("eth0", mac_address);
    sprintf(_mdns_http_config.name, "%sHTTP-%02X%02X%02X", CONFIG_MDNS_NAME, mac_address[3], mac_address[4], mac_address[5]);
    snprintf(_mdns_http_config.reg_type, sizeof(_mdns_http_config.reg_type), "%s", CONFIG_MDNS_REG_TYPE);
    _mdns_http_config.port = CONFIG_MDNS_HTTP_PORT;

    TXTRecordCreate(&_mdns_http_config.txtRecord, 0, NULL);

    const char * key1 = "version";
    const char * value1 = CONFIG_REDFISH_VERSION;
    TXTRecordSetValue(&_mdns_http_config.txtRecord, key1, strlen(value1), value1);

    const char * key2 = "path";
    const char * value2 = "/redfish/v1/";
    TXTRecordSetValue(&_mdns_http_config.txtRecord, key2, strlen(value2), value2);

    const char * key3 = "uuid";
    char uuid[37];
    redfish_get_uuid(uuid, sizeof(uuid));
    TXTRecordSetValue(&_mdns_http_config.txtRecord, key3, strlen(uuid), uuid);

    const char * key4 = "Protocol";
    const char * value4 = "http";
    TXTRecordSetValue(&_mdns_http_config.txtRecord, key4, strlen(value4), value4);

    info(tag, "mDNS service init for HTTP: %s", _mdns_http_config.name);
    info(tag, "reg_type: %s", _mdns_http_config.reg_type);
    info(tag, "port: %d", _mdns_http_config.port);
    info(tag, "version: %s", value1);
    info(tag, "uuid: %s", uuid);
    info(tag, "path: %s", value2);

    // Start mDNS service for HTTP.
    mdns_service_run(&_mdns_http_config);

    /* mDNS service init for HTTPS */
    sprintf(_mdns_https_config.name, "%sHTTPS-%02X%02X%02X", CONFIG_MDNS_NAME, mac_address[3], mac_address[4], mac_address[5]);
    snprintf(_mdns_https_config.reg_type, sizeof(_mdns_https_config.reg_type), "%s", CONFIG_MDNS_REG_TYPE);
    _mdns_https_config.port = CONFIG_MDNS_HTTPS_PORT;

    TXTRecordCreate(&_mdns_https_config.txtRecord, 0, NULL);

    TXTRecordSetValue(&_mdns_https_config.txtRecord, key1, strlen(value1), value1);
    TXTRecordSetValue(&_mdns_https_config.txtRecord, key2, strlen(value2), value2);
    TXTRecordSetValue(&_mdns_https_config.txtRecord, key3, strlen(uuid), uuid);

    const char * key5 = "Protocol";
    const char * value5 = "https";
    TXTRecordSetValue(&_mdns_https_config.txtRecord, key5, strlen(value5), value5);

    info(tag, "mDNS service init for HTTPS: %s", _mdns_https_config.name);
    info(tag, "reg_type: %s", _mdns_https_config.reg_type);
    info(tag, "port: %d", _mdns_https_config.port);
    info(tag, "version: %s", value1);
    info(tag, "uuid: %s", uuid);
    info(tag, "path: %s", value2);

    // Start mDNS service for HTTPS.
    mdns_service_run(&_mdns_https_config);
#endif

	// thread
	if (_thread_handle == NULL) {
		platform_task_create(_redfish_manager_process, "redfish_server_task", 20 * 1024 * 1024, (void *)NULL, 0, &_thread_handle);
	}

	return ret;
}

int redfish_deinit(void)
{
    int ret = SUCCESS;
    
    // Stop mDNS services
    mdns_service_stop(&_mdns_http_config);
    mdns_service_stop(&_mdns_https_config);
    
    // Clean up thread
    _thread_aborted = TRUE;
    if (_thread_handle != NULL) {
        platform_task_cancel(_thread_handle);
        _thread_handle = NULL;
    }
    
    // Clean up mutex
    if (_mutex_handle != NULL) {
        mutex_delete(_mutex_handle);
        _mutex_handle = NULL;
    }
    
    return ret;
}
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// Server Configuration
#define DEFAULT_PORT 8443
#define DEFAULT_HTTP_PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 40960
#define MAX_PATH_LENGTH 256
#define MAX_HEADERS 20
#define MAX_HEADER_NAME_LEN 64
#define MAX_HEADER_VALUE_LEN 256

// Protocol Configuration
#define SUPPORT_HTTP true
#define SUPPORT_HTTPS true

// TLS Configuration
#define TLS_CIPHER_SUITE MBEDTLS_TLS_RSA_WITH_AES_256_CBC_SHA
#define TLS_VERSION MBEDTLS_SSL_VERSION_TLS1_2
#define TLS_VERIFY_MODE MBEDTLS_SSL_VERIFY_OPTIONAL

// Certificate Configuration
#define DEFAULT_CERT_FILE "./redfish_certification/server/server.crt"
#define DEFAULT_KEY_FILE "./redfish_certification/server/server.key"
#define CLIENT_CA_FILE "./redfish_certification/root_ca/ca.crt"

// Redfish Configuration
#define REDFISH_VERSION "1.20.0"
#define REDFISH_PROTOCOL_VERSION "1.1.0"
#define MAX_REDFISH_RESOURCES 100
#define MAX_JSON_SIZE 40960

// Resource IDs
#define CHASSIS_ID "1"
#define SYSTEM_ID "1"
#define MANAGER_ID "1"
#define MANAGER_ID_KENMEC "Kenmec"

// HTTP Status Codes
#define HTTP_OK 200
#define HTTP_CREATED 201
#define HTTP_ACCEPTED 202
#define HTTP_NO_CONTENT 204
#define HTTP_TEMPORARY_REDIRECT 307
#define HTTP_BAD_REQUEST 400
#define HTTP_UNAUTHORIZED 401
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_PRECONDITION_FAILED 412
#define HTTP_INTERNAL_SERVER_ERROR 500
#define HTTP_NOT_IMPLEMENTED 501

// HTTP Methods
#define HTTP_METHOD_GET "GET"
#define HTTP_METHOD_POST "POST"
#define HTTP_METHOD_PUT "PUT"
#define HTTP_METHOD_PATCH "PATCH"
#define HTTP_METHOD_DELETE "DELETE"
#define HTTP_METHOD_HEAD "HEAD"

// Content Types
#define CONTENT_TYPE_JSON "application/json"
#define CONTENT_TYPE_TEXT "text/plain"
#define CONTENT_TYPE_HTML "text/html"

// Debug Configuration
#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

// Error handling
#define SUCCESS 0
#define ERROR_GENERAL -1
#define ERROR_INVALID_PARAM -2
#define ERROR_MEMORY -3
#define ERROR_TLS -4
#define ERROR_NETWORK -5
#define ERROR_NOT_INITIALIZED -6
#define ERROR_REQUIRES_HTTPS -7

// Platform-specific configurations
#ifdef __linux__
#define PLATFORM_LINUX
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <pthread.h>
#endif

// mbedTLS includes
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/certs.h>
#include <mbedtls/x509.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/timing.h>
#include <mbedtls/debug.h>

// JSON parsing (using local cJSON header)
#include "cJSON.h"

#endif // CONFIG_H
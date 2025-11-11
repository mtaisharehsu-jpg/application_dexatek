#ifndef TLS_SERVER_H
#define TLS_SERVER_H

#include "../include/config.h"
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cache.h>
#include <mbedtls/ssl_ciphersuites.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

// TLS server context structure (global, not per-connection)
typedef struct {
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cert;
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context pkey;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    int server_fd;
    bool initialized;
} tls_server_context_t;

// Function declarations
int tls_server_init(tls_server_context_t *ctx, const char *cert_file, const char *key_file, const char *client_cert_file, int port);
void tls_server_cleanup(tls_server_context_t *ctx);
int tls_server_start(tls_server_context_t *ctx);
int tls_server_accept_client(tls_server_context_t *ctx, int *client_fd);
int tls_server_establish_ssl(tls_server_context_t *ctx, int client_fd, mbedtls_ssl_context *ssl, mbedtls_net_context *client_net_ctx);
int tls_server_read(mbedtls_ssl_context *ssl, char *buffer, size_t buffer_size);
int tls_server_write(mbedtls_ssl_context *ssl, const char *data, size_t data_size);
void tls_server_close_client(mbedtls_ssl_context *ssl, int client_fd);

// Error handling
const char* tls_error_string(int error_code);

#endif // TLS_SERVER_H
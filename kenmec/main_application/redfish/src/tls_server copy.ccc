#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "config.h"
#include "redfish_client_info_handle.h"
#include "tls_server.h"

// Forward declaration
int handle_client_connection(int client_fd);

// Global TLS server context
static tls_server_context_t g_tls_ctx;

const char* tls_error_string(int error_code) {
    static char error_buffer[256];
    mbedtls_strerror(error_code, error_buffer, sizeof(error_buffer));
    return error_buffer;
}

int tls_server_init(tls_server_context_t *ctx, const char *cert_file, const char *key_file, const char *client_cert_file, int port) {
    int ret;
    (void)g_tls_ctx;

    if (!ctx || !cert_file || !key_file) {
        return ERROR_INVALID_PARAM;
    }

    // Initialize mbedTLS structures
    mbedtls_ssl_config_init(&ctx->conf);
    mbedtls_x509_crt_init(&ctx->cert);
    mbedtls_x509_crt_init(&ctx->client_cert);
    mbedtls_pk_init(&ctx->pkey);
    mbedtls_entropy_init(&ctx->entropy);
    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);

    // Seed the random number generator
    ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func, &ctx->entropy,
                               (const unsigned char *) "RedfishDemo", 11);
    if (ret != 0) {
        printf("Failed to seed random number generator: %s\n", tls_error_string(ret));
        return ERROR_TLS;
    }

    // Load certificate
    // Try to load from database first
    char pem_server_cert[8192];
    int server_cert_loaded = 0;
    if (system_certificate_load_pem(pem_server_cert, sizeof(pem_server_cert)) == 0) {
        printf("Loading server certificate from database...\n");
        ret = mbedtls_x509_crt_parse(&ctx->cert, (const unsigned char*)pem_server_cert, strlen(pem_server_cert) + 1);
        if (ret == 0) {
            server_cert_loaded = 1;
            printf("Successfully loaded server certificate from database\n");
            printf("Server certificate: \n\n%s\n\n", pem_server_cert);
        } else {
            printf("Failed to parse server certificate from database: %s\n", tls_error_string(ret));
        }
    }

    // Fall back to file if database certificate failed or not available
    if (server_cert_loaded == 0) {
        ret = mbedtls_x509_crt_parse_file(&ctx->cert, cert_file);
        if (ret != 0) {
            printf("Failed to load certificate: %s\n", tls_error_string(ret));
            return ERROR_TLS;
        }
        else {
            printf("Successfully loaded certificate from file: %s\n", cert_file);
        }
    }

    // Load private key - try database first, then fall back to file
    char pem_key[4096];
    int key_loaded = 0;
    
    // Try to load from database first
    if (system_private_key_load_pem(pem_key, sizeof(pem_key)) == 0) {
        printf("Loading private key from database...\n");
        ret = mbedtls_pk_parse_key(&ctx->pkey, (const unsigned char*)pem_key, strlen(pem_key) + 1, NULL, 0);
        if (ret == 0) {
            key_loaded = 1;
            printf("Successfully loaded private key from database\n");
            printf("Private key: \n\n%s\n\n", pem_key);
        } else {
            printf("Failed to parse private key from database: %s\n", tls_error_string(ret));
        }
    }
    
    // Fall back to file if database key failed or not available
    if (!key_loaded) {
        printf("Loading private key from file: %s\n", key_file);
        ret = mbedtls_pk_parse_keyfile(&ctx->pkey, key_file, NULL);
        if (ret != 0) {
            printf("Failed to load private key from file: %s\n", tls_error_string(ret));
            return ERROR_TLS;
        }
        printf("Successfully loaded private key from file\n");
    }

    // Load client certificate - try database first, then fall back to file
    char pem_client_cert[8192];
    int client_cert_loaded = 0;
    if (system_root_certificate_load_pem(pem_client_cert, sizeof(pem_client_cert)) == 0) {
        printf("Loading client certificate from database...\n");
        ret = mbedtls_x509_crt_parse(&ctx->client_cert, (const unsigned char*)pem_client_cert, strlen(pem_client_cert) + 1);
        if (ret == 0) {
            client_cert_loaded = 1;
            printf("Successfully loaded client certificate from database\n");
            printf("Client certificate: \n\n%s\n\n", pem_client_cert);
        } else {
            printf("Failed to parse client certificate from database: %s\n", tls_error_string(ret));
        }
    }
    
    // Fall back to file if database certificate failed or not available
    if (!client_cert_loaded) {
        printf("Loading client certificate from file: %s\n", client_cert_file);
        ret = mbedtls_x509_crt_parse_file(&ctx->client_cert, client_cert_file);
        if (ret != 0) {
            printf("Failed to load client CA certificate from file: %s\n", tls_error_string(ret));
            return ERROR_TLS;
        }
        printf("Successfully loaded client certificate from file\n");
    }
    

    // Configure SSL
    ret = mbedtls_ssl_config_defaults(&ctx->conf, MBEDTLS_SSL_IS_SERVER,
                                     MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        printf("Failed to configure SSL defaults: %s\n", tls_error_string(ret));
        return ERROR_TLS;
    }

    // Set certificate and private key
    ret = mbedtls_ssl_conf_own_cert(&ctx->conf, &ctx->cert, &ctx->pkey);
    if (ret != 0) {
        printf("Failed to set certificate: %s\n", tls_error_string(ret));
        return ERROR_TLS;
    }

    // Set client certificate and authentication mode
    if (client_cert_file != NULL) {
        mbedtls_ssl_conf_ca_chain(&ctx->conf, &ctx->client_cert, NULL);
    }

    // Set authentication mode
    // Config to enable client certificate authentication account to database.
    security_policy_t policy;
    ret = security_policy_get(MANAGER_ID_KENMEC, &policy);
    printf("Security policy ret: %d val: %d **************\n", ret, policy.verify_certificate);
    if (ret == 0) {
        if (policy.verify_certificate == 1) {
            mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        } else {
            mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
        }
    }
    else {
        mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    }

    // Set random number generator
    mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);

    // Set cipher suites
    mbedtls_ssl_conf_ciphersuites(&ctx->conf, mbedtls_ssl_list_ciphersuites());

    // Set minimum TLS version
    mbedtls_ssl_conf_min_version(&ctx->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_2);

    // Set maximum TLS version
    mbedtls_ssl_conf_max_version(&ctx->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);

    // Create socket
    ctx->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->server_fd < 0) {
        printf("Failed to create socket: %s\n", strerror(errno));
        return ERROR_NETWORK;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(ctx->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Failed to set socket options: %s\n", strerror(errno));
        close(ctx->server_fd);
        return ERROR_NETWORK;
    }

    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(ctx->server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Failed to bind socket: %s\n", strerror(errno));
        close(ctx->server_fd);
        return ERROR_NETWORK;
    }

    // Listen for connections
    if (listen(ctx->server_fd, MAX_CLIENTS) < 0) {
        printf("Failed to listen on socket: %s\n", strerror(errno));
        close(ctx->server_fd);
        return ERROR_NETWORK;
    }

    ctx->initialized = true;
    printf("TLS server initialized on port %d\n", port);

    return SUCCESS;
}

void tls_server_cleanup(tls_server_context_t *ctx) {
    if (!ctx) return;

    if (ctx->server_fd >= 0) {
        close(ctx->server_fd);
        ctx->server_fd = -1;
    }

    mbedtls_ssl_config_free(&ctx->conf);
    mbedtls_x509_crt_free(&ctx->cert);
    mbedtls_x509_crt_free(&ctx->client_cert);
    mbedtls_pk_free(&ctx->pkey);
    mbedtls_entropy_free(&ctx->entropy);
    mbedtls_ctr_drbg_free(&ctx->ctr_drbg);

    ctx->initialized = false;
}

int tls_server_start(tls_server_context_t *ctx) {
    if (!ctx || !ctx->initialized) {
        return ERROR_INVALID_PARAM;
    }

    printf("TLS server started, waiting for connections...\n");

    while (1) {
        int client_fd;
        int ret = tls_server_accept_client(ctx, &client_fd);
        if (ret != SUCCESS) {
            continue;
        }

        // Handle client connection in a new thread or process
        handle_client_connection(client_fd);
    }

    return SUCCESS;
}

int tls_server_accept_client(tls_server_context_t *ctx, int *client_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    *client_fd = accept(ctx->server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (*client_fd < 0) {
        printf("Failed to accept client connection: %s\n", strerror(errno));
        return ERROR_NETWORK;
    }

    printf("Client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    return SUCCESS;
}


int tls_server_establish_ssl(tls_server_context_t *ctx, int client_fd, mbedtls_ssl_context *ssl, mbedtls_net_context *client_net_ctx) {
    (void)client_fd;
    int ret;

    // Initialize SSL context for this connection
    mbedtls_ssl_init(ssl);

    // Use the global SSL config (which is already properly configured)
    ret = mbedtls_ssl_setup(ssl, &ctx->conf);
    if (ret != 0) {
        printf("Failed to setup SSL: %s\n", tls_error_string(ret));
        return ERROR_TLS;
    }

    // Set the underlying socket using proper mbedTLS BIO functions
    mbedtls_ssl_set_bio(ssl, client_net_ctx, mbedtls_net_send, mbedtls_net_recv, NULL);

    // Perform SSL handshake
    ret = mbedtls_ssl_handshake(ssl);
    if (ret != 0) {
        printf("SSL handshake failed: %s\n", tls_error_string(ret));
        return ERROR_TLS;
    }

    printf("SSL handshake completed successfully\n");
    return SUCCESS;
}

int tls_server_read(mbedtls_ssl_context *ssl, char *buffer, size_t buffer_size) {
    if (!ssl || !buffer) {
        return ERROR_INVALID_PARAM;
    }

    // Use mbedTLS SSL read. Read up to buffer_size bytes (callers may be reading
    // into the middle of a larger buffer and managing termination themselves).
    int bytes_read = mbedtls_ssl_read(ssl, (unsigned char*)buffer, buffer_size);
    
    if (bytes_read < 0) {
        if (bytes_read == MBEDTLS_ERR_SSL_WANT_READ || bytes_read == MBEDTLS_ERR_SSL_WANT_WRITE) {
            // Non-blocking operation, try again - return a special value to indicate retry needed
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
        
        // Don't print expected connection closure errors
        if (bytes_read == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
            bytes_read == MBEDTLS_ERR_NET_CONN_RESET) {
            return ERROR_NETWORK;
        }
        
        // Only print actual errors
        printf("Failed to read from SSL: %s\n", tls_error_string(bytes_read));
        return ERROR_NETWORK;
    }

    // Ensure safe null termination for callers that expect C-strings.
    // If the buffer was completely filled, we can't place a terminator without
    // overwriting data; callers should rely on the returned length in that case.
    if (bytes_read >= 0 && bytes_read < (int)buffer_size) {
        buffer[bytes_read] = '\0';
    }

    return bytes_read;
}

int tls_server_write(mbedtls_ssl_context *ssl, const char *data, size_t data_size) {
    if (!ssl || !data) {
        return ERROR_INVALID_PARAM;
    }

    // Use mbedTLS SSL write
    int bytes_written = mbedtls_ssl_write(ssl, (const unsigned char*)data, data_size);
    if (bytes_written < 0) {
        if (bytes_written == MBEDTLS_ERR_SSL_WANT_READ || bytes_written == MBEDTLS_ERR_SSL_WANT_WRITE) {
            // Non-blocking operation, try again
            return 0;
        }
        printf("Failed to write to SSL: %s\n", tls_error_string(bytes_written));
        return ERROR_NETWORK;
    }

    return bytes_written;
}

void tls_server_close_client(mbedtls_ssl_context *ssl, int client_fd) {
    if (client_fd >= 0) {
        // Close SSL connection gracefully
        int ret = mbedtls_ssl_close_notify(ssl);
        if (ret != 0 && ret != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            // Only print if it's not an expected peer close
            printf("SSL close notification failed: %s\n", tls_error_string(ret));
        }
        close(client_fd);
    }

    // Clean up SSL context
    mbedtls_ssl_free(ssl);
}
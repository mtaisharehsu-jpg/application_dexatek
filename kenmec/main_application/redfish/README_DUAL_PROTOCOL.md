# Redfish Server with Dual Protocol Support (HTTP + HTTPS)

## Overview

The Redfish server has been enhanced to support both HTTP and HTTPS protocols simultaneously. This allows clients to connect using either protocol based on their requirements and security needs.

## Features

- **HTTP Server**: Runs on port 8080 (configurable)
- **HTTPS Server**: Runs on port 8443 (configurable) with TLS/SSL encryption
- **Unified Request Processing**: Both protocols use the same Redfish request processing logic
- **Multiplexed I/O**: Uses `select()` to handle both servers efficiently in a single thread

## Configuration

### Port Configuration
- HTTP Port: `DEFAULT_HTTP_PORT` (8080) in `config.h`
- HTTPS Port: `DEFAULT_PORT` (8443) in `config.h`

### Protocol Support
- `SUPPORT_HTTP`: Enable/disable HTTP server (default: true)
- `SUPPORT_HTTPS`: Enable/disable HTTPS server (default: true)

### Certificate Configuration
- Certificate file: `DEFAULT_CERT_FILE`
- Private key file: `DEFAULT_KEY_FILE`
- Client CA file: `CLIENT_CA_FILE`

## Architecture

### Server Initialization
1. **HTTP Server**: Creates a standard TCP socket on port 8080
2. **HTTPS Server**: Initializes TLS server with certificates on port 8443
3. **Multiplexing**: Uses `select()` to monitor both server sockets

### Connection Handling
- **HTTP Connections**: Handled by `handle_http_client_connection()`
- **HTTPS Connections**: Handled by `handle_https_client_connection()` (uses existing TLS implementation)

### Request Processing
Both protocols use the same request processing pipeline:
1. Parse HTTP request (`parse_http_request()`)
2. Process Redfish request (`process_redfish_request()`)
3. Generate HTTP response (`generate_http_response()`)
4. Send response to client

## Files Modified

### Core Files
- `src/redfish_init.c`: Main server initialization and multiplexing logic
- `src/redfish_server.c`: HTTP server implementation and connection handling
- `include/redfish_server.h`: New function declarations for HTTP/HTTPS support
- `include/config.h`: Added HTTP port and protocol support configuration

### New Functions
- `http_server_init()`: Initialize HTTP server
- `http_server_get_fd()`: Get HTTP server file descriptor
- `http_server_cleanup()`: Cleanup HTTP server
- `handle_http_client_connection()`: Handle HTTP client connections
- `handle_https_client_connection()`: Handle HTTPS client connections

## Testing

### Test Script
Use the provided test script to verify both protocols:
```bash
./test_dual_protocol.sh
```

### Manual Testing
```bash
# Test HTTP
curl http://localhost:8080/redfish/v1/

# Test HTTPS (skip certificate verification for testing)
curl -k https://localhost:8443/redfish/v1/
```

## Security Considerations

### HTTP (Port 8080)
- **Use Case**: Internal networks, development, testing
- **Security**: No encryption, data transmitted in plain text
- **Recommendation**: Use only in trusted environments

### HTTPS (Port 8443)
- **Use Case**: Production environments, external access
- **Security**: TLS/SSL encryption, certificate-based authentication
- **Recommendation**: Use for all production deployments

## Deployment Options

### Development/Testing
- Enable both protocols for flexibility
- Use HTTP for quick testing, HTTPS for security validation

### Production
- Disable HTTP (`SUPPORT_HTTP false`) for security
- Use only HTTPS with proper certificates
- Configure firewall to block port 8080

### Internal Networks
- Use HTTP for internal management interfaces
- Use HTTPS for external access

## Troubleshooting

### Common Issues

1. **Port Already in Use**
   - Check if another service is using ports 8080 or 8443
   - Change port configuration in `config.h`

2. **Certificate Issues**
   - Verify certificate and key file paths
   - Check certificate validity and permissions

3. **Connection Refused**
   - Ensure server is running
   - Check firewall settings
   - Verify port configuration

### Debug Information
The server provides detailed logging for:
- Server initialization
- Connection acceptance
- Request processing
- Error conditions

## Future Enhancements

- **HTTP/2 Support**: Add HTTP/2 protocol support
- **Load Balancing**: Distribute requests across multiple server instances
- **Rate Limiting**: Implement request rate limiting per protocol
- **Metrics**: Add protocol-specific metrics and monitoring 
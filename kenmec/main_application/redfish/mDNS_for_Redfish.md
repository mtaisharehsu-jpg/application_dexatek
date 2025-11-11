# mDNS Service for Redfish

This document explains the mDNS (Multicast DNS) service discovery implementation for the Redfish API service.

## Overview

The Redfish service implements mDNS service discovery to enable automatic discovery of both HTTP and HTTPS endpoints by network clients. Two separate mDNS service instances are created - one for HTTP and one for HTTPS - allowing clients to discover and connect to the appropriate protocol endpoint.

## Service Configuration

### Service Discovery Parameters

Both HTTP and HTTPS services use the same base configuration but differ in their service names, ports, and protocol identification:

- **Service Type**: `_redfish._tcp`
- **Domain**: `local`
- **Interface**: Network interface (e.g., `enx006f00010281`) supporting both IPv4 and IPv6
- **Base Name**: Configured via `CONFIG_MDNS_NAME` (default: "Kenmec")
- **MAC Suffix**: Last 3 bytes of eth0 MAC address in hexadecimal format

### Service Instances

#### HTTPS Redfish Service

```
Service Type: _redfish._tcp
Service Name: Kenmec-HTTPS-F9E902
Domain Name: local
Interface: enx006f00010281 IPv6
Address: Dexatek-2.local/192.168.1.231:443
TXT path = /redfish/v1/
TXT version = 1.20.0
TXT Protocol = https
TXT uuid = 543c12e0-e706-4a84-87f2-0e910c95c3db
```

#### HTTP Redfish Service

```
Service Type: _redfish._tcp
Service Name: Kenmec-HTTP-F9E902
Domain Name: local
Interface: enx006f00010281 IPv6
Address: Dexatek-2.local/192.168.1.231:80
TXT path = /redfish/v1/
TXT version = 1.20.0
TXT Protocol = http
TXT uuid = 543c12e0-e706-4a84-87f2-0e910c95c3db
```

## Implementation Details

### Service Name Generation

The service names are automatically generated using the following pattern:

```c
// For HTTPS
sprintf(_mdns_https_config.name, "%sHTTPS-%02X%02X%02X", CONFIG_MDNS_NAME, mac_address[3], mac_address[4], mac_address[5]);

// For HTTP  
sprintf(_mdns_http_config.name, "%sHTTP-%02X%02X%02X", CONFIG_MDNS_NAME, mac_address[3], mac_address[4], mac_address[5]);
```

Where:
- `CONFIG_MDNS_NAME`: Base service name (e.g., "Kenmec")
- `mac_address[3-5]`: Last 3 bytes of eth0 MAC address
- Protocol identifier: "HTTPS" or "HTTP"

### TXT Record Attributes

Both services include identical TXT records except for the Protocol field:

| Attribute | Description | Example Value |
|-----------|-------------|---------------|
| `path` | Redfish API root path | `/redfish/v1/` |
| `version` | Redfish service version | `1.20.0` |
| `Protocol` | Service protocol | `https` or `http` |
| `uuid` | Unique device identifier | `543c12e0-e706-4a84-87f2-0e910c95c3db` |

### Port Configuration

- **HTTP Service**: Uses `CONFIG_MDNS_HTTP_PORT` (default: 80)
- **HTTPS Service**: Uses `CONFIG_MDNS_HTTPS_PORT` (default: 443)

## Configuration Options

### Build-time Configuration

The mDNS service is controlled by the following configuration options:

```c
#define CONFIG_MDNS_ENABLE          1           // Enable/disable mDNS service
#define CONFIG_MDNS_NAME            "Kenmec"    // Base service name
#define CONFIG_MDNS_REG_TYPE        "_redfish._tcp"  // Service type
#define CONFIG_MDNS_HTTP_PORT       80          // HTTP port
#define CONFIG_MDNS_HTTPS_PORT      443         // HTTPS port
#define CONFIG_REDFISH_VERSION      "1.20.0"    // Redfish version
```

### Runtime Behavior

1. **Service Registration**: Both services are registered during `redfish_init()`
2. **MAC Address Detection**: Automatically retrieves eth0 MAC address for unique naming
3. **UUID Generation**: Creates or retrieves persistent device UUID
4. **Service Announcement**: Services are announced on the local network immediately after registration

## Service Discovery Workflow

### For Client Applications

1. **Discovery**: Clients perform mDNS query for `_redfish._tcp.local`
2. **Service Enumeration**: Both HTTP and HTTPS services are returned
3. **Protocol Selection**: Clients choose appropriate protocol based on TXT `Protocol` field
4. **Connection**: Clients connect to the resolved IP address and port

### Example Discovery Tools

#### Using `avahi-browse` (Linux)
```bash
# Discover all Redfish services
avahi-browse -rt _redfish._tcp

# Discover specific service
avahi-browse -rt _redfish._tcp | grep Kenmec
```

#### Using `dns-sd` (macOS/Windows)
```bash
# Browse for Redfish services
dns-sd -B _redfish._tcp

# Lookup specific service
dns-sd -L "Kenmec-HTTPS-F9E902" _redfish._tcp
```

## Network Requirements

### Multicast Support
- Network infrastructure must support multicast traffic
- Firewall rules should allow mDNS traffic on UDP port 5353
- IPv6 should be enabled for full functionality

### Address Resolution
- Device must have valid IPv4/IPv6 addresses
- Hostname resolution should be properly configured
- Network interface must be in UP state

## Troubleshooting

### Common Issues

1. **Services Not Discovered**
   - Check if `CONFIG_MDNS_ENABLE` is set to 1
   - Verify network interface is UP and has valid IP address
   - Ensure multicast is supported on network

2. **Duplicate Service Names**
   - Service names include MAC address suffix for uniqueness
   - If MAC cannot be retrieved, service registration may fail

3. **TXT Record Issues**
   - UUID generation failure will result in empty uuid field
   - Version information comes from build-time configuration

### Debug Information

The service logs detailed information during initialization:

```
2025-08-01 10:40:45.708985 | INFO  | redfish | redfish_init:307 | mDNS service init for HTTP: Kenmec-HTTP-F9E902
2025-08-01 10:40:45.709047 | INFO  | redfish | redfish_init:308 | reg_type: _redfish._tcp
2025-08-01 10:40:45.709072 | INFO  | redfish | redfish_init:309 | port: 80
2025-08-01 10:40:45.709089 | INFO  | redfish | redfish_init:310 | version: 1.20.0
2025-08-01 10:40:45.709110 | INFO  | redfish | redfish_init:311 | uuid: 543c12e0-e706-4a84-87f2-0e910c95c3db
2025-08-01 10:40:45.709127 | INFO  | redfish | redfish_init:312 | path: /redfish/v1/
2025-08-01 10:40:45.709158 | DEBUG | mdns | mdns_service_run:44 | [mdns_service_run] +
2025-08-01 10:40:45.711080 | INFO  | redfish | redfish_init:332 | mDNS service init for HTTPS: Kenmec-HTTPS-F9E902
2025-08-01 10:40:45.711141 | INFO  | redfish | redfish_init:333 | reg_type: _redfish._tcp
2025-08-01 10:40:45.711162 | INFO  | redfish | redfish_init:334 | port: 443
2025-08-01 10:40:45.711178 | INFO  | redfish | redfish_init:335 | version: 1.20.0
2025-08-01 10:40:45.711194 | INFO  | redfish | redfish_init:336 | uuid: 543c12e0-e706-4a84-87f2-0e910c95c3db
2025-08-01 10:40:45.711210 | INFO  | redfish | redfish_init:337 | path: /redfish/v1/
2025-08-01 10:40:45.711232 | DEBUG | mdns | mdns_service_run:44 | [mdns_service_run] +
```

# Redfish Server Testing Commands

This document provides comprehensive testing commands for the Redfish server, including both HTTP and HTTPS endpoints.

## Table of Contents
- [Prerequisites](#prerequisites)
- [GET Requests](#get-requests)
- [POST Requests](#post-requests)
- [PATCH Requests](#patch-requests)
- [DELETE Requests](#delete-requests)
- [Authentication Testing](#authentication-testing)
- [Error Testing](#error-testing)
- [Performance Testing](#performance-testing)

## Prerequisites

### Certificate Setup
Ensure you have the required certificates in the correct locations:
```bash
# Certificate directory structure
redfish_certification/
├── root_ca/
│   └── ca.crt
├── server/
│   ├── server.crt
│   └── server.key
└── client/
    ├── client.crt
    └── client.key
```

### Server Configuration
- **HTTP Server**: Port 8080
- **HTTPS Server**: Port 8443
- **Default IP**: 127.0.0.1 (adjust as needed)

## GET Requests

### Service Root
```bash
# HTTP
curl -X GET http://127.0.0.1:8080/redfish/v1/  | python3 -m json.tool

# HTTP with Basic Authentication (Authorization header)
curl -X GET http://127.0.0.1:8080/redfish/v1/ \
     -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" \
| python3 -m json.tool

# HTTPS
curl --cert client/client.crt --key client/client.key --cacert root_ca/ca.crt \
     -X GET https://127.0.0.1:8443/redfish/v1/
```

### Account Management
```bash
# Account Service
curl -X GET http://127.0.0.1:8080/redfish/v1/AccountService -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" | python3 -m json.tool

# Account Service Accounts
curl -X GET 'http://127.0.0.1:8080/redfish/v1/AccountService/Accounts' -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" | python3 -m json.tool

# Account Service Accounts Members
curl -X GET http://127.0.0.1:8080/redfish/v1/AccountService/Accounts/1 -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" | python3 -m json.tool

# Account Service Account Delete
curl -X DELETE http://127.0.0.1:8080/redfish/v1/AccountService/Accounts/1 -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" | python3 -m json.tool
```

### System Resources
```bash
# Systems Collection
curl -X GET http://127.0.0.1:8080/redfish/v1/Systems

# Specific System
curl -X GET http://127.0.0.1:8080/redfish/v1/Systems/1
```

### Manager Resources
```bash
# Managers Collection
curl -X GET http://127.0.0.1:8080/redfish/v1/Managers | python3 -m json.tool

# Specific Manager
curl -X GET http://127.0.0.1:8080/redfish/v1/Managers/Dexatek

# Manager Ethernet Interfaces
curl -X GET http://127.0.0.1:8080/redfish/v1/Managers/Dexatek/EthernetInterfaces

# Specific Ethernet Interface
curl -X GET http://127.0.0.1:8080/redfish/v1/Managers/Dexatek/EthernetInterfaces/eth0

# HTTPS version with mTLS
curl --cert client/client.crt --key client/client.key --cacert root_ca/ca.crt \
     -X GET https://127.0.0.1:8443/redfish/v1/Managers/Dexatek/EthernetInterfaces/eth0
```

### Session Management
```bash
# Sessions Collection
curl -X GET http://127.0.0.1:8080/redfish/v1/SessionService/Sessions | python3 -m json.tool

# Specific Session (requires authentication)
curl -X GET http://127.0.0.1:8080/redfish/v1/SessionService/Sessions/1 \
     -H "X-Auth-Token: cwFbbZD1Xlk1TychjRyg54ZCwtQmr01G861zYrKVtxHliJa2tDxpkz8Xrf0hPaZ"
```

## POST Requests

### Account Management
```bash
# Create Account (HTTP)
curl -X POST http://127.0.0.1:8080/redfish/v1/AccountService/Accounts \
     -H "Content-Type: application/json" \
     -d '{
           "UserName": "kelvin",
           "Password": "kelvin123",
           "RoleId": "ReadOnly",
           "Enabled": true
         }'

# Create Account (HTTPS)
curl --cert client/client.crt --key client/client.key --cacert root_ca/ca.crt \
     -X POST https://127.0.0.1:8443/redfish/v1/AccountService/Accounts \
     -H "Content-Type: application/json" \
     -d '{
           "UserName": "admin",
           "Password": "admin123",
           "RoleId": "Administrator",
           "Enabled": true
         }'
```

### Session Creation
```bash
# Create Session (HTTP)
curl -X POST http://127.0.0.1:8080/redfish/v1/SessionService/Sessions \
     -H "Content-Type: application/json" \
     -d '{"UserName": "admin", "Password": "admin123"}' | python3 -m json.tool

# Create Session (HTTPS)
curl --cacert root_ca/ca.crt \
     -X POST https://127.0.0.1:8443/redfish/v1/SessionService/Sessions \
     -H "Content-Type: application/json" \
     -d '{"UserName": "admin", "Password": "admin123"}'
```

## PATCH Requests

### Ethernet Interface Configuration
```bash
# Configure Static IP
curl -X PATCH http://127.0.0.1:8080/redfish/v1/Managers/Dexatek/EthernetInterfaces/eth0 \
     -H "Content-Type: application/json" \
     -H "X-Auth-Token: YOUR_SESSION_TOKEN" \
     -d '{
           "IPv4Addresses": [
             {
               "Address": "192.168.82.81",
               "AddressOrigin": "Static",
               "Gateway": "192.168.82.1",
               "SubnetMask": "255.255.255.0"
             }
           ]
         }'

# Configure DHCP
curl -X PATCH http://127.0.0.1:8080/redfish/v1/Managers/Dexatek/EthernetInterfaces/eth0 \
     -H "Content-Type: application/json" \
     -H "X-Auth-Token: YOUR_SESSION_TOKEN" \
     -d '{
           "IPv4Addresses": [
             {
               "AddressOrigin": "DHCP"
             }
           ]
         }'

# HTTPS version with mTLS
curl --cert client/client.crt --key client/client.key --cacert root_ca/ca.crt \
     -X PATCH https://127.0.0.1:8443/redfish/v1/Managers/Dexatek/EthernetInterfaces/eth0 \
     -H "Content-Type: application/json" \
     -H "X-Auth-Token: YOUR_SESSION_TOKEN" \
     -d '{
           "IPv4Addresses": [
             {
               "Address": "192.168.82.82",
               "AddressOrigin": "Static",
               "Gateway": "192.168.82.1",
               "SubnetMask": "255.255.255.0"
             }
           ]
         }'


# HTTPS version with mTLS and mDNS
curl -v --cacert root_ca/ca.crt \
  --cert client/client.crt \
  --key client/client.key \
  --connect-to Dexatek.local:8443:127.0.0.1:8443 \
  https://Dexatek.local:8443/redfish/v1

Note: 127.0.0.1 is the IP address of the server. Need to change according to the actual server IP.


curl -v --cacert root_ca/ca.crt \
  --cert client/client.crt \
  --key client/client.key \
  --connect-to Dexatek.local:8443:127.0.0.1:8443 \
  -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" \
  -X GET https://Dexatek.local:8443/redfish/v1/Managers/Kenmec/EthernetInterfaces/eth0


# $metadata
curl -X GET 'http://127.0.0.1:8080/redfish/v1/$metadata'   -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)"

# redfish/v1/Managers/
curl -X GET 'http://127.0.0.1:8080/redfish/v1/Managers'   -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)"

# redfish/v1/Managers/Kenmec
curl -X GET 'http://127.0.0.1:8080/redfish/v1/Managers/Kenmec'   -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" | python3 -m json.tool

# redfish/v1/Managers/Kenmec/EthernetInterfaces
curl -X GET 'http://127.0.0.1:8080/redfish/v1/Managers/Kenmec/EthernetInterfaces'   -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" | python3 -m json.tool

# redfish/v1/Managers/Kenmec/EthernetInterfaces/eth0
curl -X GET 'http://127.0.0.1:8080/redfish/v1/Managers/Kenmec/EthernetInterfaces/eth0'   -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" | python3 -m json.tool


# ServiceSession POST
curl -v --cacert root_ca/ca.crt   --connect-to Dexatek.local:8443:127.0.0.1:8443   -X POST   -H "Content-Type: application/json"   -d '{"UserName": "admin", "Password": "admin123"}'   https://Dexatek.local:8443/redfish/v1/SessionService/Sessions


# Test the protocol validator
```bash
clear && cd /home/kelvin/Kenmec/augentix-platform-application/application_dexatek/kenmec/main_application/redfish/Redfish-Protocol-Validator && source .venv/bin/activate && python rf_protocol_validator.py --user admin --password admin123 --rhost http://127.0.0.1:8080 --no-cert-check --log-level DEBUG
```

# Redfish Protocol Validator with HTTP

```bash
clear && cd /home/kelvin/Kenmec/augentix-platform-application/application_dexatek/kenmec/main_application/redfish/Redfish-Protocol-Validator && source .venv/bin/activate && python rf_protocol_validator.py --user admin --password admin123 --rhost http://127.0.0.1:8080  --ca-bundle /home/kelvin/Kenmec/augentix-platform-application/application_dexatek/kenmec/main_application/redfish/redfish_certification/root_ca/ca.crt --log-level DEBUG
```

# Redfish Protocol Validator with HTTPS

```bash
clear && cd /home/kelvin/Kenmec/augentix-platform-application/application_dexatek/kenmec/main_application/redfish/Redfish-Protocol-Validator && source .venv/bin/activate && python rf_protocol_validator.py --user admin --password admin123 --rhost https://127.0.0.1:8443  --ca-bundle /home/kelvin/Kenmec/augentix-platform-application/application_dexatek/kenmec/main_application/redfish/redfish_certification/root_ca/ca.crt --log-level DEBUG
```

# Redfish Service Validator with HTTPS
### Single
```bash
clear && cd /home/kelvin/Kenmec/augentix-platform-application/application_dexatek/kenmec/main_application/redfish/Redfish-Service-Validator/ && python3 RedfishServiceValidator.py -u admin -p admin123  -i https://127.0.0.1:8443 --payload Single /redfish/v1/Systems
```

### Entire Service
```bash
clear && cd /home/kelvin/Kenmec/augentix-platform-application/application_dexatek/kenmec/main_application/redfish/Redfish-Service-Validator/ && python3 RedfishServiceValidator.py -u admin -p admin123  -i https://127.0.0.1:8443
```



curl --cacert root_ca/ca.crt   -i   -X POST https://127.0.0.1:8443/redfish/v1/SessionService/Sessions      -H "Content-Type: application/json"      -d '{"UserName": "kelvin", "Password": "kelvin123"}'

curl --cacert root_ca/ca.crt   -i   -X GET https://127.0.0.1:8443/redfish/v1/SessionService/Sessions      -H "X-Auth-Token: Fg1gN8GJ9OFLtgbUqE7924hmr45fgyMj3aeUMBopoujlVwbyUkpL75kNAxPZ3NB"

curl --cacert root_ca/ca.crt   -i   -X DELETE https://127.0.0.1:8443/redfish/v1/SessionService/Sessions/2      -H "X-Auth-Token: RrGddOlltGkIsAi4QHMo5VI7H9S9MpeRe36N5lZvhvKxksblY1LWPoqDHfioTLb"


curl -X POST http://127.0.0.1:8080/redfish/v1/AccountService/Accounts -i \
     -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" \
     -H "Content-Type: application/json" \
     -d '{
           "UserName": "kelvin",
           "Password": "kelvin123",
           "RoleId": "ReadOnly",
           "Enabled": true
         }'

curl -i -s -u admin:admin123 \
  -H "Content-Type: application/json" \
  -X POST \
  -d '{"Pid": 162, "TimeoutMs": 500}' \
  http://127.0.0.1:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/IOBoards/1/Actions/Oem/KenmecIOBoard.Write


# GET Manager Security Policy 
```bash
curl -v --cacert root_ca/ca.crt   --cert client/client.crt   --key client/client.key   --connect-to Dexatek.local:8443:127.0.0.1:8443   -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)"   -X GET https://Dexatek.local:8443/redfish/v1/Managers/Kenmec/SecurityPolicy | python3 -m json.tool
```

# PATCH Manager Security Policy
```bash
  curl --cacert root_ca/ca.crt \
  --cert client/client.crt \
  --key client/client.key \
  --connect-to Dexatek.local:8443:127.0.0.1:8443 \
  -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" \
  -H "Content-Type: application/json" \
  -X PATCH https://Dexatek.local:8443/redfish/v1/Managers/Kenmec/SecurityPolicy \
  -d '{"TLS":{"Server":{"VerifyCertificate":true}}}'
```

# Reset System
```bash
curl -v --cacert root_ca/ca.crt   --cert client/client.crt   --key client/client.key   --connect-to Dexatek.local:8443:127.0.0.1:8443   -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)"   -X POST https://Dexatek.local:8443/redfish/v1/Managers/Kenmec/Actions/Manager.Reset | python3 -m json.tool
```

# Generate CSR
```bash
curl -v --cacert root_ca/ca.crt --cert client/client.crt --key client/client.key --connect-to Dexatek.local:8443:127.0.0.1:8443 -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)" -H "Content-Type: application/json" -X POST https://Dexatek.local:8443/redfish/v1/CertificateService/Actions/CertificateService.GenerateCSR -d '{ "CertificateCollection": { "@odata.id": "/redfish/v1/Managers/Kenmec/NetworkProtocol/HTTPS/Certificates" }, "Country": "TW", "State": "Taipei", "City": "Taipei", "Organization": "Dexatek", "OrganizationalUnit": "IT Department", "CommonName": "Dexatek.local", "AlternativeNames": ["Dexatek.local", "127.0.0.1"], "KeyUsage": ["KeyEncipherment", "DigitalSignature", "ServerAuthentication"], "KeyPairAlgorithm": "TPM_ALG_ECDSA", "KeyBitLength": 2048, "HashAlgorithm": "SHA256" }' | python3 -m json.tool
```


# Update Firmware
### Single
```bash
curl -v -X POST 'http://127.0.0.1:8080/UpdateFirmwareMultipart'   -H "Authorization: Basic $(printf '%s' 'admin:admin123' | base64)"   -F "file=@OTA_TEST_FIRMWARE1.swu;type=application/octet-stream"
```

### Multipart HTTP
```bash
curl -k -u admin:admin123 \
  -X POST http://127.0.0.1:8080/UpdateFirmwareMultipart \
  -H "Content-Type: multipart/form-data" \
  -F "UpdateFile=@OTA_TEST_FIRMWARE1.swu;type=application/octet-stream" \
  -F "UpdateParameters={\"@Redfish.OperationApplyTime\":\"Immediate\"};type=application/json"
```

### Multipart HTTPS
```bash
curl -k -u admin:admin123 \
  -X POST https://127.0.0.1:8443/UpdateFirmwareMultipart \
  -H "Content-Type: multipart/form-data" \
  -F "UpdateFile=@OTA_TEST_FIRMWARE1.swu;type=application/octet-stream" \
  -F "UpdateParameters={\"@Redfish.OperationApplyTime\":\"Immediate\"};type=application/json"
```
# Redfish mTLS Certificate Generation Guide

This guide provides step-by-step instructions for generating mutual TLS (mTLS) certificates for Redfish server and client authentication.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Root CA Generation](#root-ca-generation)
- [Server Certificate Generation](#server-certificate-generation)
- [Client Certificate Generation](#client-certificate-generation)
- [Certificate Verification](#certificate-verification)
- [Usage in Redfish](#usage-in-redfish)

## Prerequisites

Before starting, ensure you have OpenSSL installed:
```bash
# Ubuntu/Debian
sudo apt-get install openssl

# CentOS/RHEL
sudo yum install openssl

# macOS
brew install openssl
```

## Root CA Generation

### Step 1: Generate Root CA Private Key
```bash
openssl genrsa -out ca.key 2048
```

### Step 2: Generate Root CA Certificate
```bash
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt \
  -subj "/C=TW/ST=Taipei/L=Taipei/O=Dexatek/OU=IT/CN=Dexatek Root CA 2024"
```

**Parameters Explained:**
- `-x509`: Generate a self-signed certificate
- `-new`: Create a new certificate
- `-nodes`: No DES encryption (no password)
- `-key ca.key`: Use the private key
- `-sha256`: Use SHA256 for signing
- `-days 3650`: Valid for 10 years
- `-subj`: Certificate subject information

## Server Certificate Generation

### Step 1: Create Server OpenSSL Configuration
Create `server.conf`:
```ini
[ req ]
default_bits       = 2048
prompt             = no
default_md         = sha256
distinguished_name = dn
req_extensions     = req_ext

[ dn ]
C = TW
ST = Taipei
L = Taipei
O = Dexatek
OU = IT Department
CN = Dexatek.local

[ req_ext ]
subjectAltName = @alt_names
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth

[ alt_names ]
DNS.1 = Dexatek.local
```

### Step 2: Generate Server Private Key
```bash
openssl genrsa -out server.key 2048
```

### Step 3: Generate Server Certificate Signing Request (CSR)
```bash
openssl req -new -key server.key -out server.csr -config server.conf
```

### Step 4: Sign Server Certificate with Root CA
```bash
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days 365 -sha256 -extfile server.conf -extensions req_ext
```

## Client Certificate Generation

### Step 1: Create Client OpenSSL Configuration
Create `client.conf`:
```ini
[ req ]
default_bits       = 2048
prompt             = no
default_md         = sha256
distinguished_name = dn
req_extensions     = req_ext

[ dn ]
C = TW
ST = Taipei
L = Taipei
O = Dexatek
OU = IT Department
CN = redfish-client

[ req_ext ]
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
```

### Step 2: Generate Client Private Key
```bash
openssl genrsa -out client.key 2048
```

### Step 3: Generate Client Certificate Signing Request (CSR)
```bash
openssl req -new -key client.key -out client.csr -config client.conf
```

### Step 4: Create Client Extension File
Create `client_ext.cnf`:
```ini
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
```

### Step 5: Sign Client Certificate with Root CA
```bash
openssl x509 -req -in client.csr \
  -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out client.crt -days 365 -sha256 \
  -extfile client_ext.cnf
```

## Certificate Verification

### Verify Root CA Certificate
```bash
openssl x509 -in ca.crt -text -noout
```

### Verify Server Certificate
```bash
openssl x509 -in server.crt -text -noout
```

### Verify Client Certificate
```bash
openssl x509 -in client.crt -text -noout
```

### Verify Certificate Chain
```bash
# Verify server certificate against root CA
openssl verify -CAfile ca.crt server.crt

# Verify client certificate against root CA
openssl verify -CAfile ca.crt client.crt
```

## Complete Generation Workflow

### Quick Setup Commands
```bash
# Navigate to redfish_certification directory
cd redfish_certification

# 1. Generate Root CA
openssl genrsa -out root_ca/ca.key 2048
openssl req -x509 -new -nodes -key root_ca/ca.key -sha256 -days 3650 -out root_ca/ca.crt \
  -subj "/C=TW/ST=Taipei/L=Taipei/O=Dexatek/OU=IT/CN=Dexatek Root CA 2025"

# 2. Generate Server Certificate
openssl genrsa -out server/server.key 2048
openssl req -new -key server/server.key -out server/server.csr -config server/server.conf
openssl x509 -req -in server/server.csr -CA root_ca/ca.crt -CAkey root_ca/ca.key -CAcreateserial \
  -out server/server.crt -days 365 -sha256 -extfile server/server.conf -extensions req_ext

# 3. Generate Client Certificate
openssl genrsa -out client/client.key 2048
openssl req -new -key client/client.key -out client/client.csr -config client/client.conf
openssl x509 -req -in client/client.csr -CA root_ca/ca.crt -CAkey root_ca/ca.key -CAcreateserial \
  -out client/client.crt -days 365 -sha256 -extfile client/client_ext.cnf

# 4. Verify Certificates
openssl verify -CAfile root_ca/ca.crt server/server.crt
openssl verify -CAfile root_ca/ca.crt client/client.crt
```

## Usage in Redfish

### Directory Structure
```
redfish_certification/
├── root_ca/
│   ├── ca.key
│   └── ca.crt
├── server/
│   ├── server.key
│   ├── server.csr
│   ├── server.crt
│   └── server.conf
└── client/
    ├── client.key
    ├── client.csr
    ├── client.crt
    ├── client.conf
```

### Configuration in Redfish Server
Update your Redfish server configuration to use the generated certificates:

```c
// In config.h or similar
#define DEFAULT_CERT_FILE "./redfish_certification/server/server.crt"
#define DEFAULT_KEY_FILE "./redfish_certification/server/server.key"
#define CLIENT_CA_FILE "./redfish_certification/root_ca/ca.crt"
```

### Testing mTLS Connection
```bash
# Test server with client certificate
curl --cacert ca.crt --cert client.crt --key client.key \
  https://Dexatek.local:8443/redfish/v1/

# With hostname resolution
curl --cacert ca.crt --cert client.crt --key client.key \
  https://Dexatek.local:8443/redfish/v1/ \
  --resolve Dexatek.local:8443:192.168.1.231
```

## Security Best Practices

1. **Keep Private Keys Secure**: Store private keys in a secure location with restricted access
2. **Regular Certificate Rotation**: Replace certificates before expiration
3. **Strong Key Sizes**: Use at least 2048-bit RSA keys
4. **Proper Subject Names**: Use meaningful CN and SAN values
5. **Certificate Revocation**: Implement CRL or OCSP for certificate revocation

## Troubleshooting

### Common Issues

1. **Certificate Verification Failed**
   - Ensure the certificate chain is complete
   - Check that the root CA is trusted
   - Verify certificate dates are valid

2. **Hostname Mismatch**
   - Update SAN (Subject Alternative Name) in server certificate
   - Ensure DNS names and IP addresses match

3. **Permission Denied**
   - Check file permissions on private keys
   - Ensure proper ownership of certificate files

### Useful Commands

```bash
# Check certificate expiration
openssl x509 -in certificate.crt -noout -dates

# Convert certificate formats
openssl x509 -in certificate.crt -out certificate.pem -outform PEM

# View certificate details
openssl x509 -in certificate.crt -text -noout | grep -A 10 "Subject Alternative Name"
```

## Notes

- All certificates are valid for 365 days (1 year) except the root CA (10 years)
- The root CA certificate should be distributed to all clients
- Server and client certificates are signed by the root CA
- Update IP addresses and hostnames in the configuration files as needed





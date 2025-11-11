#!/usr/bin/env python3
import requests
import base64
import json
import datetime
from cryptography import x509
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.x509.oid import ExtendedKeyUsageOID

# Target via connect-to: hit 127.0.0.1:8080 but keep Host= Dexatek.local
URL = "http://127.0.0.1:8080/redfish/v1/CertificateService/Actions/CertificateService.GenerateCSR"
REPLACE_URL = "http://127.0.0.1:8080/redfish/v1/CertificateService/Actions/CertificateService.ReplaceCertificate"
HOST_HEADER = "Dexatek.local"

# Local CA files for signing the CSR
CA_CERT_PATH = "ca.crt"
CA_KEY_PATH = "ca.key"  # must exist and match ca.crt
CA_KEY_PASSWORD = None  # set bytes password if the key is encrypted, e.g., b"secret"

auth = base64.b64encode(b"admin:admin123").decode("ascii")
headers = {
    "Authorization": f"Basic {auth}",
    "Content-Type": "application/json",
    "Host": HOST_HEADER,
}

payload = {
    "CertificateCollection": {
        "@odata.id": "/redfish/v1/Managers/Kenmec/NetworkProtocol/HTTPS/Certificates"
    },
    "Country": "TW",
    "State": "Taipei",
    "City": "Taipei",
    "Organization": "Dexatek",
    "OrganizationalUnit": "IT Department",
    "CommonName": "Dexatek.local",
    "AlternativeNames": ["Dexatek.local", "127.0.0.1"],
    "KeyUsage": ["KeyEncipherment", "DigitalSignature", "ServerAuthentication"],
    "KeyPairAlgorithm": "TPM_ALG_ECDSA",
    "KeyBitLength": 2048,
    "HashAlgorithm": "SHA256",
}

print("\n===== GENERATE CSR REQUEST =====\n")
print(json.dumps(payload, indent=4))
resp = requests.post(URL, headers=headers, data=json.dumps(payload), timeout=30)
resp.raise_for_status()
resp_json = resp.json()
print("\n===== GENERATE CSR RESPONSE =====\n")
print(json.dumps(resp_json, indent=4))

# Parse CSRString
pem = resp_json.get("CSRString", "")
if pem:
    csr = x509.load_pem_x509_csr(pem.encode("utf-8"), default_backend())
    print("\nSubject DN:", csr.subject.rfc4514_string())

    # SANs
    san_val = None
    try:
        san = csr.extensions.get_extension_for_class(x509.SubjectAlternativeName).value
        san_val = san
        print("SANs:", [x.value for x in san])
    except x509.ExtensionNotFound:
        print("SANs: (none)")

    # Key Usage
    ku_val = None
    try:
        ku = csr.extensions.get_extension_for_class(x509.KeyUsage).value
        ku_val = ku
        usages = []
        if ku.digital_signature: usages.append("DigitalSignature")
        if ku.content_commitment: usages.append("ContentCommitment")
        if ku.key_encipherment: usages.append("KeyEncipherment")
        if ku.data_encipherment: usages.append("DataEncipherment")
        if ku.key_agreement:
            usages.append("KeyAgreement")
            if ku.encipher_only: usages.append("EncipherOnly")
            if ku.decipher_only: usages.append("DecipherOnly")
        if ku.key_cert_sign: usages.append("KeyCertSign")
        if ku.crl_sign: usages.append("CRLSign")
        print("KeyUsage:", usages if usages else "(none)")
    except x509.ExtensionNotFound:
        print("KeyUsage: (none)")

    # EKU
    eku_val = None
    try:
        eku = csr.extensions.get_extension_for_class(x509.ExtendedKeyUsage).value
        eku_val = eku
        eku_map = {
            ExtendedKeyUsageOID.SERVER_AUTH.dotted_string: "ServerAuth",
            ExtendedKeyUsageOID.CLIENT_AUTH.dotted_string: "ClientAuth",
            ExtendedKeyUsageOID.CODE_SIGNING.dotted_string: "CodeSigning",
            ExtendedKeyUsageOID.EMAIL_PROTECTION.dotted_string: "EmailProtection",
            ExtendedKeyUsageOID.TIME_STAMPING.dotted_string: "TimeStamping",
            ExtendedKeyUsageOID.OCSP_SIGNING.dotted_string: "OCSPSigning",
            ExtendedKeyUsageOID.ANY_EXTENDED_KEY_USAGE.dotted_string: "AnyExtendedKeyUsage",
        }
        human = [eku_map.get(oid.dotted_string, oid.dotted_string) for oid in eku]
        print("ExtendedKeyUsage:", human)
    except x509.ExtensionNotFound:
        print("ExtendedKeyUsage: (none)")

    # Load CA cert/key
    def load_ca(ca_cert_path: str, ca_key_path: str, password=None):
        with open(ca_cert_path, "rb") as f:
            ca_cert = x509.load_pem_x509_certificate(f.read(), default_backend())
        with open(ca_key_path, "rb") as f:
            ca_key = serialization.load_pem_private_key(f.read(), password=password, backend=default_backend())
        return ca_cert, ca_key

    ca_cert, ca_key = load_ca(CA_CERT_PATH, CA_KEY_PATH, CA_KEY_PASSWORD)

    # Build certificate from CSR
    builder = (
        x509.CertificateBuilder()
        .subject_name(csr.subject)
        .issuer_name(ca_cert.subject)
        .public_key(csr.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(datetime.datetime.utcnow() - datetime.timedelta(minutes=1))
        .not_valid_after(datetime.datetime.utcnow() + datetime.timedelta(days=365))
    )

    # Copy CSR extensions
    if san_val is not None:
        builder = builder.add_extension(san_val, critical=False)
    if ku_val is not None:
        builder = builder.add_extension(ku_val, critical=True)
    if eku_val is not None:
        builder = builder.add_extension(eku_val, critical=False)
    # Ensure basic constraints CA:FALSE
    builder = builder.add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)

    cert = builder.sign(private_key=ca_key, algorithm=hashes.SHA256(), backend=default_backend())
    issued_pem = cert.public_bytes(serialization.Encoding.PEM).decode("utf-8")
    print("\n===== ISSUED CERTIFICATE (PEM) =====\n")
    print(issued_pem)

    # Generate JSON payload with CertificateString
    replace_payload = {
        "CertificateUri": {
            "@odata.id": "/redfish/v1/Managers/Kenmec/NetworkProtocol/HTTPS/Certificates/1"
        },
        "CertificateString": issued_pem,
        "CertificateType": "PEM",
    }
    print("\n===== REPLACE CERTIFICATE REQUEST =====\n")
    print(json.dumps(replace_payload, indent=4))

    # POST ReplaceCertificate action
    print("\n===== REPLACE CERTIFICATE RESPONSE =====\n")
    rep = requests.post(REPLACE_URL, headers=headers, data=json.dumps(replace_payload), timeout=30)
    try:
        rep.raise_for_status()
        print(json.dumps(rep.json(), indent=4))
    except Exception:
        print(f"HTTP {rep.status_code}\n{rep.text}")


    # POST ReplaceCertificate action for root certificate
    print("\n===== REPLACE ROOT CERTIFICATE REQUEST =====\n")
    replace_payload_root = {
        "CertificateUri": {
            "@odata.id": "/redfish/v1/Managers/Kenmec/SecurityPolicy/TLS/Server/TrustedCertificates/1"
        },
        "CertificateString": ca_cert.public_bytes(serialization.Encoding.PEM).decode("utf-8"),
        "CertificateType": "PEM",
    }
    print(json.dumps(replace_payload_root, indent=4))

    print("\n===== REPLACE ROOT CERTIFICATE RESPONSE =====\n")
    
    rep = requests.post(REPLACE_URL, headers=headers, data=json.dumps(replace_payload_root), timeout=30)
    try:
        rep.raise_for_status()
        print(json.dumps(rep.json(), indent=4))
    except Exception:
        print(f"HTTP {rep.status_code}\n{rep.text}")
else:
    print("No CSRString in response")
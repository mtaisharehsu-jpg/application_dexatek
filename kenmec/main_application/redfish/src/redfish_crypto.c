#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "redfish_crypto.h"

#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/x509_csr.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/oid.h>
#include <mbedtls/ecp.h>
#include <strings.h>

static int map_md_alg(const char *hash_alg)
{
	if (!hash_alg) return MBEDTLS_MD_SHA256;
	if (strcasecmp(hash_alg, "SHA256") == 0) return MBEDTLS_MD_SHA256;
	if (strcasecmp(hash_alg, "SHA384") == 0) return MBEDTLS_MD_SHA384;
	if (strcasecmp(hash_alg, "SHA512") == 0) return MBEDTLS_MD_SHA512;
	return MBEDTLS_MD_SHA256;
}

static void append_dn_field(char *dst, size_t dst_size, const char *prefix, const char *value)
{
	if (!value || value[0] == '\0') return;
	size_t cur = strlen(dst);
	if (cur > 0 && cur < dst_size - 1) {
		dst[cur++] = ',';
		dst[cur] = '\0';
	}
	snprintf(dst + strlen(dst), dst_size - strlen(dst), "%s=%s", prefix, value);
}

static void build_subject_dn(const certificate_csr_request_t *req, char *subject, size_t subject_size)
{
	subject[0] = '\0';
	append_dn_field(subject, subject_size, "C", req->country[0] ? req->country : NULL);
	append_dn_field(subject, subject_size, "ST", req->state[0] ? req->state : NULL);
	append_dn_field(subject, subject_size, "L", req->city[0] ? req->city : NULL);
	append_dn_field(subject, subject_size, "O", req->organization[0] ? req->organization : NULL);
	append_dn_field(subject, subject_size, "OU", req->organizational_unit[0] ? req->organizational_unit : NULL);
	append_dn_field(subject, subject_size, "CN", req->common_name[0] ? req->common_name : NULL);
}

// Minimal DER helpers
static size_t der_len_len(size_t len) {
	if (len < 128) return 1;
	else if (len < 256) return 2;
	else if (len < 65536) return 3;
	else return 4;
}
static size_t der_write_len(unsigned char *p, size_t len) {
	if (len < 128) { p[0] = (unsigned char)len; return 1; }
	if (len < 256) { p[0] = 0x81; p[1] = (unsigned char)len; return 2; }
	if (len < 65536) { p[0] = 0x82; p[1] = (unsigned char)(len >> 8); p[2] = (unsigned char)(len & 0xFF); return 3; }
	p[0] = 0x83; p[1] = (unsigned char)((len >> 16) & 0xFF); p[2] = (unsigned char)((len >> 8) & 0xFF); p[3] = (unsigned char)(len & 0xFF); return 4;
}

static int write_san_extension(const certificate_csr_request_t *req, mbedtls_x509write_csr *wcsr)
{
	// Build GeneralNames SEQUENCE from req->alternative_names
	size_t inner_len = 0;
	for (int i = 0; i < req->alternative_name_count; i++) {
		const char *s = req->alternative_names[i];
		if (!s || !*s) continue;
		unsigned char ipbuf[16];
		int is_ip_len = 0;
		if (inet_pton(AF_INET, s, ipbuf) == 1) is_ip_len = 4;
		else if (inet_pton(AF_INET6, s, ipbuf) == 1) is_ip_len = 16;
		if (is_ip_len > 0) {
			inner_len += 1 + der_len_len((size_t)is_ip_len) + (size_t)is_ip_len; // [7] iPAddress
		} else {
			size_t n = strlen(s);
			inner_len += 1 + der_len_len(n) + n; // [2] dNSName
		}
	}
	if (inner_len == 0) return 0; // no SANs, not an error

	size_t seq_len_len = der_len_len(inner_len);
	size_t total = 1 + seq_len_len + inner_len;
	unsigned char *san = (unsigned char *) malloc(total);
	if (!san) return -1;
	size_t pos = 0;
	san[pos++] = 0x30; // SEQUENCE
	pos += der_write_len(san + pos, inner_len);
	for (int i = 0; i < req->alternative_name_count; i++) {
		const char *s = req->alternative_names[i];
		if (!s || !*s) continue;
		unsigned char ipbuf[16];
		int iplen = 0;
		if (inet_pton(AF_INET, s, ipbuf) == 1) iplen = 4;
		else if (inet_pton(AF_INET6, s, ipbuf) == 1) iplen = 16;
		if (iplen > 0) {
			san[pos++] = 0x87; // [7] iPAddress
			pos += der_write_len(san + pos, (size_t)iplen);
			memcpy(san + pos, ipbuf, (size_t)iplen);
			pos += (size_t)iplen;
		} else {
			size_t n = strlen(s);
			san[pos++] = 0x82; // [2] dNSName
			pos += der_write_len(san + pos, n);
			memcpy(san + pos, s, n);
			pos += n;
		}
	}
	int xret = mbedtls_x509write_csr_set_extension(wcsr,
					MBEDTLS_OID_SUBJECT_ALT_NAME,
					MBEDTLS_OID_SIZE(MBEDTLS_OID_SUBJECT_ALT_NAME),
					san, total);
	free(san);
	return xret;
}

static unsigned char map_key_usage_bits(const certificate_csr_request_t *req)
{
	unsigned char bits = 0;
	for (int i = 0; i < req->key_usage_count; i++) {
		const char *ku = req->key_usage[i];
		if (!ku) continue;
		if (strcasecmp(ku, "DigitalSignature") == 0) bits |= MBEDTLS_X509_KU_DIGITAL_SIGNATURE;
		if (strcasecmp(ku, "NonRepudiation") == 0 || strcasecmp(ku, "ContentCommitment") == 0) bits |= MBEDTLS_X509_KU_NON_REPUDIATION;
		if (strcasecmp(ku, "KeyEncipherment") == 0) bits |= MBEDTLS_X509_KU_KEY_ENCIPHERMENT;
		if (strcasecmp(ku, "DataEncipherment") == 0) bits |= MBEDTLS_X509_KU_DATA_ENCIPHERMENT;
		if (strcasecmp(ku, "KeyAgreement") == 0) bits |= MBEDTLS_X509_KU_KEY_AGREEMENT;
		if (strcasecmp(ku, "KeyCertSign") == 0) bits |= MBEDTLS_X509_KU_KEY_CERT_SIGN;
		if (strcasecmp(ku, "CRLSign") == 0) bits |= MBEDTLS_X509_KU_CRL_SIGN;
	}
	return bits;
}

static int set_eku_server_auth(mbedtls_x509write_csr *wcsr, int need_server_auth)
{
	if (!need_server_auth) return 0;
	// Build SEQUENCE of one OID: id-kp-serverAuth
	size_t inner_len = 1 + der_len_len(MBEDTLS_OID_SIZE(MBEDTLS_OID_SERVER_AUTH)) + MBEDTLS_OID_SIZE(MBEDTLS_OID_SERVER_AUTH);
	size_t total = 1 + der_len_len(inner_len) + inner_len;
	unsigned char *eku = (unsigned char *) malloc(total);
	if (!eku) return -1;
	size_t pos = 0;
	eku[pos++] = 0x30; // SEQUENCE
	pos += der_write_len(eku + pos, inner_len);
	eku[pos++] = 0x06; // OID
	pos += der_write_len(eku + pos, MBEDTLS_OID_SIZE(MBEDTLS_OID_SERVER_AUTH));
	memcpy(eku + pos, MBEDTLS_OID_SERVER_AUTH, MBEDTLS_OID_SIZE(MBEDTLS_OID_SERVER_AUTH));
	pos += MBEDTLS_OID_SIZE(MBEDTLS_OID_SERVER_AUTH);
	int xret = mbedtls_x509write_csr_set_extension(wcsr,
					MBEDTLS_OID_EXTENDED_KEY_USAGE,
					MBEDTLS_OID_SIZE(MBEDTLS_OID_EXTENDED_KEY_USAGE),
					eku, total);
	free(eku);
	return xret;
}

int redfish_generate_csr(const certificate_csr_request_t *request,
                         char *csr_out,
                         size_t csr_out_size)
{
	if (!request || !csr_out || csr_out_size == 0) return -1;
	csr_out[0] = '\0';

	int ret = 0;
	char subject[512];
	build_subject_dn(request, subject, sizeof(subject));
	if (subject[0] == '\0') {
		// CN at minimum
		return -2;
	}

	mbedtls_pk_context pk;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	const char *pers = "redfish_gen_csr";

	mbedtls_pk_init(&pk);
	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&ctr_drbg);

	ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
				(const unsigned char *)pers, strlen(pers));
	if (ret != 0) { ret = -10; goto cleanup; }

	int use_ecdsa = 1;
	if (request->key_pair_algorithm[0]) {
		if (strstr(request->key_pair_algorithm, "RSA") || strstr(request->key_pair_algorithm, "TPM_ALG_RSA")) use_ecdsa = 0;
		if (strstr(request->key_pair_algorithm, "ECDSA") || strstr(request->key_pair_algorithm, "TPM_ALG_ECDSA")) use_ecdsa = 1;
	}

	if (use_ecdsa) {
		ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
		if (ret != 0) { ret = -11; goto cleanup; }
		mbedtls_ecp_group_id gid = MBEDTLS_ECP_DP_SECP256R1;
		if (request->key_bit_length >= 384) gid = MBEDTLS_ECP_DP_SECP384R1;
		ret = mbedtls_ecp_gen_key(gid, mbedtls_pk_ec(pk), mbedtls_ctr_drbg_random, &ctr_drbg);
		if (ret != 0) { ret = -12; goto cleanup; }
	} else {
		int bits = request->key_bit_length > 0 ? request->key_bit_length : 2048;
		ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
		if (ret != 0) { ret = -13; goto cleanup; }
		ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(pk), mbedtls_ctr_drbg_random, &ctr_drbg, bits, 65537);
		if (ret != 0) { ret = -14; goto cleanup; }
	}

	{
		unsigned char keybuf[4096];
        memset(keybuf, 0, sizeof(keybuf));
        ret = mbedtls_pk_write_key_pem(&pk, keybuf, sizeof(keybuf));
        if (ret != 0) {
            fprintf(stderr, "write_key_pem failed: -0x%04x\n", -ret);
            goto cleanup;
        }
        // store the private key to database
        ret = system_private_key_store_pem((const char *)keybuf);
        if (ret != 0) {
            fprintf(stderr, "store_private_key failed: -0x%04x\n", -ret);
            goto cleanup;
        }
	}

	// // load the private key from database for testing
	// {
	// 	char keybuf[4096];
	// 	memset(keybuf, 0, sizeof(keybuf));
	// 	ret = system_private_key_load_pem(keybuf, sizeof(keybuf));
	// 	if (ret != 0) {
	// 		fprintf(stderr, "load_private_key failed: -0x%04x\n", -ret);
	// 		goto cleanup;
	// 	}
	// 	// print the private key
	// 	printf("=======================\nprivate key: \n\"%s\"\n=======================\n", keybuf);
	// }

	mbedtls_x509write_csr wcsr;
	mbedtls_x509write_csr_init(&wcsr);
	mbedtls_x509write_csr_set_key(&wcsr, &pk);
	mbedtls_x509write_csr_set_md_alg(&wcsr, map_md_alg(request->hash_algorithm));
	ret = mbedtls_x509write_csr_set_subject_name(&wcsr, subject);
	if (ret != 0) { ret = -15; goto csr_cleanup; }

	// SANs
	if (request->alternative_name_count > 0) {
		write_san_extension(request, &wcsr);
	}
	// KeyUsage
	{
		unsigned char ku = map_key_usage_bits(request);
		if (ku) mbedtls_x509write_csr_set_key_usage(&wcsr, ku);
	}
	// Extended Key Usage: include serverAuth if present in KeyUsage list 'ServerAuthentication'
	{
		int need_server_auth = 0;
		for (int i = 0; i < request->key_usage_count; i++) {
			if (strcasecmp(request->key_usage[i], "ServerAuthentication") == 0) { need_server_auth = 1; break; }
		}
		if (need_server_auth) set_eku_server_auth(&wcsr, 1);
	}

	unsigned char buf[8192];
	memset(buf, 0, sizeof(buf));
	ret = mbedtls_x509write_csr_pem(&wcsr, buf, sizeof(buf), mbedtls_ctr_drbg_random, &ctr_drbg);
	if (ret != 0) { ret = -16; goto csr_cleanup; }

	strncpy(csr_out, (const char *)buf, csr_out_size - 1);
	csr_out[csr_out_size - 1] = '\0';
	ret = 0;

csr_cleanup:
	mbedtls_x509write_csr_free(&wcsr);
cleanup:
	mbedtls_pk_free(&pk);
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);
	return ret;
}

int redfish_escape_pem_for_json(const char *pem,
				     char *json_out,
				     size_t json_out_size)
{
	if (!pem || !json_out || json_out_size == 0) return -1;
	size_t j = 0;
	for (size_t i = 0; pem[i] != '\0'; i++) {
		unsigned char ch = (unsigned char)pem[i];
		if (ch == '\\' || ch == '"') {
			if (j + 2 >= json_out_size) break;
			json_out[j++] = '\\';
			json_out[j++] = (char)ch;
		} else if (ch == '\n') {
			if (j + 2 >= json_out_size) break;
			json_out[j++] = '\\';
			json_out[j++] = 'n';
		} else if (ch == '\r') {
			// skip CR
			continue;
		} else {
			if (j + 1 >= json_out_size) break;
			json_out[j++] = (char)ch;
		}
	}
	if (j >= json_out_size) j = json_out_size - 1;
	json_out[j] = '\0';
	return (int)j;
}

int redfish_parse_certificate_info(const char *pem_cert,
    char *issuer_country, size_t issuer_country_size,
    char *issuer_state, size_t issuer_state_size,
    char *issuer_city, size_t issuer_city_size,
    char *issuer_org, size_t issuer_org_size,
    char *issuer_ou, size_t issuer_ou_size,
    char *issuer_cn, size_t issuer_cn_size,
    char *subject_country, size_t subject_country_size,
    char *subject_state, size_t subject_state_size,
    char *subject_city, size_t subject_city_size,
    char *subject_org, size_t subject_org_size,
    char *subject_ou, size_t subject_ou_size,
    char *subject_cn, size_t subject_cn_size,
    char *valid_not_before, size_t valid_not_before_size,
    char *valid_not_after, size_t valid_not_after_size)
{
    if (!pem_cert) return -1;
    
    // Initialize all output strings to "Unknown"
    snprintf(issuer_country, issuer_country_size, "Unknown");
    snprintf(issuer_state, issuer_state_size, "Unknown");
    snprintf(issuer_city, issuer_city_size, "Unknown");
    snprintf(issuer_org, issuer_org_size, "Unknown");
    snprintf(issuer_ou, issuer_ou_size, "Unknown");
    snprintf(issuer_cn, issuer_cn_size, "Unknown");
    snprintf(subject_country, subject_country_size, "Unknown");
    snprintf(subject_state, subject_state_size, "Unknown");
    snprintf(subject_city, subject_city_size, "Unknown");
    snprintf(subject_org, subject_org_size, "Unknown");
    snprintf(subject_ou, subject_ou_size, "Unknown");
    snprintf(subject_cn, subject_cn_size, "Unknown");
    snprintf(valid_not_before, valid_not_before_size, "Unknown");
    snprintf(valid_not_after, valid_not_after_size, "Unknown");
    
    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);
    
    int ret = mbedtls_x509_crt_parse(&cert, (const unsigned char *)pem_cert, strlen(pem_cert) + 1);
    if (ret != 0) {
        mbedtls_x509_crt_free(&cert);
        return -1;
    }
    
    // Parse issuer information
    const mbedtls_x509_name *issuer = &cert.issuer;
    while (issuer != NULL) {
        if (issuer->oid.tag == MBEDTLS_ASN1_OID) {
            const char *short_name = NULL;
            if (mbedtls_oid_get_attr_short_name(&issuer->oid, &short_name) == 0 && short_name != NULL) {
                if (strcmp(short_name, "C") == 0) {
                    snprintf(issuer_country, issuer_country_size, "%.*s", 
                        (int)issuer->val.len, issuer->val.p);
                } else if (strcmp(short_name, "ST") == 0) {
                    snprintf(issuer_state, issuer_state_size, "%.*s", 
                        (int)issuer->val.len, issuer->val.p);
                } else if (strcmp(short_name, "L") == 0) {
                    snprintf(issuer_city, issuer_city_size, "%.*s", 
                        (int)issuer->val.len, issuer->val.p);
                } else if (strcmp(short_name, "O") == 0) {
                    snprintf(issuer_org, issuer_org_size, "%.*s", 
                        (int)issuer->val.len, issuer->val.p);
                } else if (strcmp(short_name, "OU") == 0) {
                    snprintf(issuer_ou, issuer_ou_size, "%.*s", 
                        (int)issuer->val.len, issuer->val.p);
                } else if (strcmp(short_name, "CN") == 0) {
                    snprintf(issuer_cn, issuer_cn_size, "%.*s", 
                        (int)issuer->val.len, issuer->val.p);
                }
            }
        }
        issuer = issuer->next;
    }
    
    // Parse subject information
    const mbedtls_x509_name *subject = &cert.subject;
    while (subject != NULL) {
        if (subject->oid.tag == MBEDTLS_ASN1_OID) {
            const char *short_name = NULL;
            if (mbedtls_oid_get_attr_short_name(&subject->oid, &short_name) == 0 && short_name != NULL) {
                if (strcmp(short_name, "C") == 0) {
                    snprintf(subject_country, subject_country_size, "%.*s", 
                        (int)subject->val.len, subject->val.p);
                } else if (strcmp(short_name, "ST") == 0) {
                    snprintf(subject_state, subject_state_size, "%.*s", 
                        (int)subject->val.len, subject->val.p);
                } else if (strcmp(short_name, "L") == 0) {
                    snprintf(subject_city, subject_city_size, "%.*s", 
                        (int)subject->val.len, subject->val.p);
                } else if (strcmp(short_name, "O") == 0) {
                    snprintf(subject_org, subject_org_size, "%.*s", 
                        (int)subject->val.len, subject->val.p);
                } else if (strcmp(short_name, "OU") == 0) {
                    snprintf(subject_ou, subject_ou_size, "%.*s", 
                        (int)subject->val.len, subject->val.p);
                } else if (strcmp(short_name, "CN") == 0) {
                    snprintf(subject_cn, subject_cn_size, "%.*s", 
                        (int)subject->val.len, subject->val.p);
                }
            }
        }
        subject = subject->next;
    }
    
    // Parse validity dates
    snprintf(valid_not_before, valid_not_before_size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
        cert.valid_from.year, cert.valid_from.mon, cert.valid_from.day,
        cert.valid_from.hour, cert.valid_from.min, cert.valid_from.sec);
    snprintf(valid_not_after, valid_not_after_size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
        cert.valid_to.year, cert.valid_to.mon, cert.valid_to.day,
        cert.valid_to.hour, cert.valid_to.min, cert.valid_to.sec);
    
    mbedtls_x509_crt_free(&cert);
    return 0;
}

#ifndef REDFISH_CRYPTO_H
#define REDFISH_CRYPTO_H

#include <stddef.h>

// Parsed CSR request attributes
typedef struct certificate_csr_request_s {
	char certificate_collection_odata_id[256];

	char country[64];
	char state[64];
	char city[64];
	char organization[128];
	char organizational_unit[128];
	char common_name[256];

	char key_pair_algorithm[64];
	int key_bit_length;
	char hash_algorithm[32];

	int alternative_name_count;
	char alternative_names[10][128];

	int key_usage_count;
	char key_usage[10][64];
} certificate_csr_request_t;

// Generate a CSR string based on the provided request parameters.
// Returns 0 on success, non-zero on error.
int redfish_generate_csr(const certificate_csr_request_t *request,
                         char *csr_out,
                         size_t csr_out_size);

// Escape a PEM string for JSON (escapes backslash, quotes, converts newlines to \n)
// Returns number of bytes written (excluding NUL) or negative on error.
int redfish_escape_pem_for_json(const char *pem,
				     char *json_out,
				     size_t json_out_size);

#endif // REDFISH_CRYPTO_H

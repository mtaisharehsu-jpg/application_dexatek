#define _GNU_SOURCE

#include "dexatek/main_application/include/application_common.h"

#include "kenmec/main_application/kenmec_config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <config.h>
#include <cJSON.h>
#include <time.h>

#include "redfish_client_info_handle.h"
#include "redfish_resources.h"

#include <openssl/rand.h>
#include <mbedtls/base64.h>


static sqlite3 *db = NULL;
// SecurityPolicy table helpers
static int ensure_security_policy_table(sqlite3 *ldb)
{
    char *err = NULL;
    int rc = SQLITE_OK;

    // Create table (no-op if exists)
    const char *sql_create =
        "CREATE TABLE IF NOT EXISTS security_policy ("
        "  manager TEXT NOT NULL PRIMARY KEY,"
        "  verify_certificate INTEGER NOT NULL DEFAULT 0"
        ");";
    rc = sqlite3_exec(ldb, sql_create, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to ensure security_policy table: %s\n", err);
        sqlite3_free(err);
        return -1;
    }

    // Best-effort migration for legacy schemas: add column if missing
    sqlite3_exec(ldb,
        "ALTER TABLE security_policy ADD COLUMN verify_certificate INTEGER NOT NULL DEFAULT 0;",
        NULL, NULL, NULL); // ignore error if already exists

    // Ensure uniqueness by index (safe even with PRIMARY KEY present)
    sqlite3_exec(ldb,
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_security_policy_manager ON security_policy(manager);",
        NULL, NULL, NULL);

    return 0;
}

int security_policy_upsert(const char *manager_id, const security_policy_t *policy)
{
    if (!manager_id || !policy) return -1;
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    if (ensure_security_policy_table(db) != 0) { sqlite3_close(db); return -1; }

    const char *sql_upsert =
        "INSERT INTO security_policy(manager, verify_certificate) VALUES(?, ?) "
        "ON CONFLICT(manager) DO UPDATE SET verify_certificate=excluded.verify_certificate;";
    const char *sql_replace =
        "INSERT OR REPLACE INTO security_policy(manager, verify_certificate) VALUES(?, ?);";

    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql_upsert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        // Fallback for older SQLite without ON CONFLICT DO UPDATE
        rc = sqlite3_prepare_v2(db, sql_replace, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare UPSERT/REPLACE security_policy: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return -1;
        }
    }
    sqlite3_bind_text(stmt, 1, manager_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, policy->verify_certificate ? 1 : 0);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int security_policy_get(const char *manager_id, security_policy_t *out_policy)
{
    if (!manager_id || !out_policy) return -1;
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    if (ensure_security_policy_table(db) != 0) { sqlite3_close(db); return -1; }

    const char *sql = "SELECT verify_certificate FROM security_policy WHERE manager = ?;";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT security_policy: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, manager_id, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    int ret = -1;
    if (rc == SQLITE_ROW) {
        int v = sqlite3_column_int(stmt, 0);
        out_policy->verify_certificate = v ? 1 : 0;
        ret = 0;
    }
    else {
        fprintf(stderr, "Failed to get security policy: %s\n", sqlite3_errmsg(db));
        ret = -1;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ret;
}

int security_policy_delete(const char *manager_id)
{
    if (!manager_id) return -1;
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    if (ensure_security_policy_table(db) != 0) { sqlite3_close(db); return -1; }

    const char *sql = "DELETE FROM security_policy WHERE manager = ?;";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare DELETE security_policy: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, manager_id, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

sqlite3* get_db_handle(void)
{
    return db;
}


void generate_secure_token(char *token, size_t length)
{
    if (length < 1) return;

    unsigned char buffer[length];

    if (RAND_bytes(buffer, length - 1) != 1) {
        fprintf(stderr, "Error generating secure random bytes.\n");
        exit(EXIT_FAILURE);
    }

    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t charset_size = sizeof(charset) - 1;

    for (size_t i = 0; i < length - 1; i++) {
        token[i] = charset[buffer[i] % charset_size];
    }
    token[length - 1] = '\0';
}


static bool is_token_valid(const char *token) 
{
    const char *sql = "SELECT expiry FROM sessions WHERE token = ?;";
    sqlite3_stmt *stmt;
    bool token_found = false;
    bool token_valid = false;

    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return false;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        token_found = true;
        
        // Get the expiry time from the database
        const unsigned char *expiry_str = sqlite3_column_text(stmt, 0);
        if (expiry_str) {
            // Parse the expiry time string
            struct tm expiry_tm = {0};
            if (strptime((const char *)expiry_str, "%Y-%m-%d %H:%M:%S", &expiry_tm) != NULL) {
                time_t expiry_time = mktime(&expiry_tm);
                time_t current_time = time(NULL);
                
                // Check if the token has expired
                if (current_time < expiry_time) {
                    token_valid = true;
                    printf("Token validation: Token is valid (expires at %s)\n", expiry_str);
                } else {
                    printf("Token validation: Token has expired (expired at %s, current time: %ld)\n", 
                           expiry_str, current_time);
                }
            } else {
                printf("Token validation: Failed to parse expiry time: %s\n", expiry_str);
            }
        } else {
            printf("Token validation: No expiry time found for token\n");
        }
    } else {
        printf("Token validation: Token not found in database\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return token_found && token_valid;
}
// Helper to get session by token
static int get_session_by_token(const char *token, char *out_username, size_t out_username_size, char *out_role, size_t out_role_size)
{
    if (!token) return -1;
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    const char *sql = "SELECT username, role FROM sessions WHERE token = ?;";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT session: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char *u = sqlite3_column_text(stmt, 0);
        const unsigned char *r = sqlite3_column_text(stmt, 1);
        if (u && out_username && out_username_size > 0) {
            strncpy(out_username, (const char *)u, out_username_size - 1);
            out_username[out_username_size - 1] = '\0';
        }
        if (r && out_role && out_role_size > 0) {
            strncpy(out_role, (const char *)r, out_role_size - 1);
            out_role[out_role_size - 1] = '\0';
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return SUCCESS;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return -1;
}

int get_authenticated_identity(const http_request_t *request,
                               char *out_username, size_t out_username_size,
                               char *out_role, size_t out_role_size)
{
    if (!request) return FAIL;

    // Try X-Auth-Token first
    for (int i = 0; i < request->header_count; i++) {
        if (strncmp(request->headers[i][0], "X-Auth-Token", 12) == 0) {
            const char *token = request->headers[i][1];
            if (is_token_valid(token) && get_session_by_token(token, out_username, out_username_size, out_role, out_role_size) == SUCCESS) {
                return SUCCESS;
            }
            break;
        }
    }

    // Fallback to Basic auth: decode and return the username; role lookup from accounts
    const char *auth_header = NULL;
    for (int i = 0; i < request->header_count; i++) {
        if (strncmp(request->headers[i][0], "Authorization", 13) == 0) {
            auth_header = request->headers[i][1];
            break;
        }
    }
    if (auth_header) {
        const char *prefix = "Basic ";
        size_t prefix_len = strlen(prefix);
        if (strncmp(auth_header, prefix, prefix_len) == 0) {
            const char *b64 = auth_header + prefix_len;
            unsigned char decoded[256];
            size_t decoded_len = 0;
            if (mbedtls_base64_decode(decoded, sizeof(decoded) - 1, &decoded_len,
                                       (const unsigned char *)b64, strlen(b64)) == 0) {
                decoded[decoded_len] = '\0';
                char *colon = strchr((char *)decoded, ':');
                if (colon) {
                    *colon = '\0';
                    const char *username = (const char *)decoded;
                    // Copy username out
                    if (out_username && out_username_size > 0) {
                        strncpy(out_username, username, out_username_size - 1);
                        out_username[out_username_size - 1] = '\0';
                    }
                    // Lookup role from accounts
                    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
                    if (rc == SQLITE_OK) {
                        const char *sql = "SELECT role FROM accounts WHERE username = ?;";
                        sqlite3_stmt *stmt = NULL;
                        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
                            if (sqlite3_step(stmt) == SQLITE_ROW) {
                                const unsigned char *role = sqlite3_column_text(stmt, 0);
                                if (role && out_role && out_role_size > 0) {
                                    strncpy(out_role, (const char *)role, out_role_size - 1);
                                    out_role[out_role_size - 1] = '\0';
                                }
                            }
                            sqlite3_finalize(stmt);
                        }
                        sqlite3_close(db);
                    }
                    return SUCCESS;
                }
            }
        }
    }

    return FAIL;
}

// Function to clean up expired sessions from the database
int cleanup_expired_sessions(void)
{
    const char *sql = "DELETE FROM sessions WHERE expiry < datetime('now', 'localtime');";
    char *err_msg = NULL;
    int rc;

    rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to cleanup expired sessions: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    int deleted_count = sqlite3_changes(db);
    if (deleted_count > 0) {
        printf("Cleaned up %d expired sessions from database\n", deleted_count);
    }

    sqlite3_close(db);
    return deleted_count;
}

int dump_all_sessions(void)
{
    const char *sql = "SELECT id, token, username, role, expiry FROM sessions ORDER BY id;";
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT sessions: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    printf("\n=== ALL SESSIONS IN DATABASE ===\n");
    printf("%-4s %-20s %-15s %-15s %-20s\n", "ID", "Token", "Username", "Role", "Expiry");
    printf("%-4s %-20s %-15s %-15s %-20s\n", "---", "-----", "--------", "----", "------");

    int session_count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *token = sqlite3_column_text(stmt, 1);
        const unsigned char *username = sqlite3_column_text(stmt, 2);
        const unsigned char *role = sqlite3_column_text(stmt, 3);
        const unsigned char *expiry = sqlite3_column_text(stmt, 4);

        printf("%-4d %-20s %-15s %-15s %-20s\n", 
               id, 
               token ? (const char*)token : "NULL",
               username ? (const char*)username : "NULL",
               role ? (const char*)role : "NULL",
               expiry ? (const char*)expiry : "NULL");
        session_count++;
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Error reading sessions: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    printf("=== Total sessions: %d ===\n\n", session_count);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return session_count;
}

int session_list_get(session_info_t* session_info_list)
{
    if (!session_info_list) {
        fprintf(stderr, "session_info_list parameter is NULL\n");
        return -1;
    }

    const char *sql = "SELECT id, token, username, role, expiry FROM sessions ORDER BY id;";
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT sessions: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        session_info_list[count].id = sqlite3_column_int(stmt, 0);
        
        const unsigned char *token = sqlite3_column_text(stmt, 1);
        if (token) {
            strncpy(session_info_list[count].token, (const char*)token, MAX_TOKEN_LENGTH - 1);
            session_info_list[count].token[MAX_TOKEN_LENGTH - 1] = '\0';
        } else {
            session_info_list[count].token[0] = '\0';
        }

        const unsigned char *username = sqlite3_column_text(stmt, 2);
        if (username) {
            strncpy(session_info_list[count].username, (const char*)username, MAX_USERNAME_LENGTH - 1);
            session_info_list[count].username[MAX_USERNAME_LENGTH - 1] = '\0';
        } else {
            session_info_list[count].username[0] = '\0';
        }

        const unsigned char *role = sqlite3_column_text(stmt, 3);
        if (role) {
            strncpy(session_info_list[count].role, (const char*)role, MAX_ROLE_LENGTH - 1);
            session_info_list[count].role[MAX_ROLE_LENGTH - 1] = '\0';
        } else {
            session_info_list[count].role[0] = '\0';
        }

        const unsigned char *expiry = sqlite3_column_text(stmt, 4);
        if (expiry) {
            strncpy(session_info_list[count].expiry, (const char*)expiry, 31);
            session_info_list[count].expiry[31] = '\0';
        } else {
            session_info_list[count].expiry[0] = '\0';
        }

        count++;
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Error reading sessions: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
}

int session_count_get(void)
{
    const char *sql = "SELECT COUNT(*) FROM sessions;";
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT COUNT: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    int count = 0;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
}


static int check_basic_auth(const http_request_t *request)
{
    if (!request) return FAIL;

    const char *auth_header = NULL;
    for (int i = 0; i < request->header_count; i++) {
        if (strncmp(request->headers[i][0], "Authorization", 13) == 0) {
            auth_header = request->headers[i][1];
            break;
        }
    }

    if (!auth_header) return FAIL;

    const char *prefix = "Basic ";
    size_t prefix_len = strlen(prefix);
    if (strncmp(auth_header, prefix, prefix_len) != 0) {
        return FAIL; // Not Basic scheme
    }

    const char *b64 = auth_header + prefix_len;
    unsigned char decoded[256];
    size_t decoded_len = 0;
    int rc = mbedtls_base64_decode(decoded, sizeof(decoded) - 1, &decoded_len,
                                   (const unsigned char *)b64, strlen(b64));
    if (rc != 0) {
        printf("Basic auth: base64 decode failed (rc=%d)\n", rc);
        return FAIL;
    }
    decoded[decoded_len] = '\0';

    char *colon = strchr((char *)decoded, ':');
    if (!colon) {
        printf("Basic auth: missing ':' separator\n");
        return FAIL;
    }

    *colon = '\0';
    const char *username = (const char *)decoded;
    const char *password = colon + 1;

    db_status_type_t st = account_check(username, password);
    return (st == SUCCESS) ? SUCCESS : FAIL;
}


int check_client_token(const http_request_t *request)
{   
    bool result = false;

    // Allow unauthenticated access ONLY for session creation (POST Sessions)
    printf("request->path: %s\n", request->path);
    if (request && (strcmp(request->path, "/redfish/v1/SessionService/Sessions") == 0) &&
        (strcmp(request->method, HTTP_METHOD_POST) == 0)) {
        return SUCCESS;
    }

    // Allow unauthenticated access for POST to Sessions Members alias
    if (request && (strcmp(request->path, "/redfish/v1/SessionService/Sessions/Members") == 0) &&
        (strcmp(request->method, HTTP_METHOD_POST) == 0)) {
        return SUCCESS;
    }

    // Allow odata and metadata endpoint
    if (request && (strcmp(request->path, "/redfish/v1/$metadata") == 0)) {
        return SUCCESS;
    }

    if (request && (strcmp(request->path, "/redfish/v1/odata") == 0)) {
        return SUCCESS;
    }

    // Allow "/redfish" endpoint
    if (request && (strcmp(request->path, "/redfish") == 0)) {
        return SUCCESS;
    }

    // Allow "/redfish/v1" endpoint
    if (request && (strcmp(request->path, "/redfish/v1/") == 0)) {
        return SUCCESS;
    }

    // Note: Do not allow unauthenticated GET to Sessions collection (HTTP or HTTPS)

    for(int index = 0; index < request->header_count; index++) {
        if(strncmp(request->headers[index][0], "X-Auth-Token", 12) == 0) {
            result = is_token_valid(request->headers[index][1]);
            break;
        }
    }

    if (result == true) {
        return SUCCESS;
    }

    // Fallback to HTTP Basic authentication if no valid token
    if (check_basic_auth(request) == SUCCESS) {
        return SUCCESS;
    }

    return FAIL;
}


int db_init(void)
{
    char *err_msg = NULL;
    int rc;

    rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s %s\n", CONFIG_REDFISH_ACCOUNT_DB_PATH, sqlite3_errmsg(db));
        return -1;
    }

    const char *sql_create_accounts_table = 
        "CREATE TABLE IF NOT EXISTS accounts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE NOT NULL,"
        "password TEXT NOT NULL,"
        "role TEXT NOT NULL,"
        "enabled BOOLEAN DEFAULT 1,"
        "locked BOOLEAN DEFAULT 0"
        ");";

    rc = sqlite3_exec(db, sql_create_accounts_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }


    const char *sql_create_sessions_table =
        "CREATE TABLE IF NOT EXISTS sessions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "token TEXT UNIQUE NOT NULL,"
        "username TEXT NOT NULL,"
        "role TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "last_accessed DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "expiry DATETIME"
        ");";

    rc = sqlite3_exec(db, sql_create_sessions_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create sessions table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    const char *sql_create_uuid_table =
        "CREATE TABLE IF NOT EXISTS system_uuid ("
        "id INTEGER PRIMARY KEY CHECK (id = 1),"
        "uuid TEXT NOT NULL"
        ");";

    rc = sqlite3_exec(db, sql_create_uuid_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create uuid table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    const char *sql_create_system_private_key_table =
        "CREATE TABLE IF NOT EXISTS system_private_key ("
        "id INTEGER PRIMARY KEY CHECK (id = 1),"
        "pem TEXT"
        ");";

    rc = sqlite3_exec(db, sql_create_system_private_key_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create system_private_key table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    const char *sql_create_system_certificate_table =
        "CREATE TABLE IF NOT EXISTS system_certificate ("
        "id INTEGER PRIMARY KEY CHECK (id = 1),"
        "pem TEXT"
        ");";

    rc = sqlite3_exec(db, sql_create_system_certificate_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create system_certificate table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    const char *sql_create_system_root_certificate_table =
        "CREATE TABLE IF NOT EXISTS system_root_certificate ("
        "id INTEGER PRIMARY KEY CHECK (id = 1),"
        "pem TEXT"
        ");";

    rc = sqlite3_exec(db, sql_create_system_root_certificate_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create system_root_certificate table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    // Seed a default admin account if the accounts table is empty
    {
        const char *sql_count_accounts = "SELECT COUNT(*) FROM accounts;";
        sqlite3_stmt *stmt_count = NULL;
        rc = sqlite3_prepare_v2(db, sql_count_accounts, -1, &stmt_count, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare COUNT(accounts): %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return -1;
        }

        rc = sqlite3_step(stmt_count);
        int accounts_count = -1;
        if (rc == SQLITE_ROW) {
            accounts_count = sqlite3_column_int(stmt_count, 0);
        } else {
            fprintf(stderr, "Failed to execute COUNT(accounts): %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt_count);
            sqlite3_close(db);
            return -1;
        }
        sqlite3_finalize(stmt_count);

        if (accounts_count == 0) {
            const char *sql_insert_default = "INSERT INTO accounts (username, password, role) VALUES (?, ?, ?);";
            sqlite3_stmt *stmt_insert = NULL;
            rc = sqlite3_prepare_v2(db, sql_insert_default, -1, &stmt_insert, NULL);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "Failed to prepare INSERT default admin: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                return -1;
            }

            const char *default_username = "admin";
            const char *default_password = "admin123";
            const char *default_role = "Administrator";

            sqlite3_bind_text(stmt_insert, 1, default_username, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt_insert, 2, default_password, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt_insert, 3, default_role, -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt_insert);
            if (rc != SQLITE_DONE) {
                fprintf(stderr, "Failed to insert default admin account: %s\n", sqlite3_errmsg(db));
                sqlite3_finalize(stmt_insert);
                sqlite3_close(db);
                return -1;
            }
            sqlite3_finalize(stmt_insert);
            printf("Seeded default admin account (username=admin).\n");
        }
    }

    return SUCCESS;
}


int account_add(const char *username, const char *password, const char *role, http_response_t *response)
{
    char *err_msg = NULL;
    int rc;

    rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to create manager\"}");
        return -1;
    }

    const char *sql_create_table = 
        "CREATE TABLE IF NOT EXISTS accounts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE NOT NULL,"
        "password TEXT NOT NULL,"
        "role TEXT NOT NULL"
        ");";

    rc = sqlite3_exec(db, sql_create_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to create table\"}");
        return -1;
    }

    // Find the smallest available account ID
    const char *sql_find_gap = "SELECT id FROM accounts ORDER BY id;";
    sqlite3_stmt *stmt_gap;
    rc = sqlite3_prepare_v2(db, sql_find_gap, -1, &stmt_gap, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT account IDs: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to prepare SELECT statement\"}");
        return -1;
    }

    int account_id = 1;
    while ((rc = sqlite3_step(stmt_gap)) == SQLITE_ROW) {
        int existing_id = sqlite3_column_int(stmt_gap, 0);
        if (existing_id == account_id) {
            account_id++; // This ID is taken, try next
        } else {
            // Found a gap, use this ID
            break;
        }
    }
    sqlite3_finalize(stmt_gap);

    // Insert account with specific ID
    const char *sql_insert = "INSERT INTO accounts (id, username, password, role) VALUES (?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare insert statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->body, "{\"error\":\"Failed to prepare insert statement\"}");
        return -1;
    }

    sqlite3_bind_int(stmt, 1, account_id);
    sqlite3_bind_text(stmt, 2, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, password, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, role, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        const char *error_msg = sqlite3_errmsg(db);
        fprintf(stderr, "Failed to insert data: %s\n", error_msg);
        
        // Check for specific constraint violations before closing database
        if (rc == SQLITE_CONSTRAINT && strstr(error_msg, "UNIQUE constraint failed: accounts.username")) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            response->status_code = HTTP_BAD_REQUEST;
            strcpy(response->content_type, "application/json");
            int body_len = snprintf(response->body, MAX_JSON_SIZE,
                "{\n"
                "  \"error\": {\n"
                "    \"code\": \"Base.1.15.0.ResourceAlreadyExists\",\n"
                "    \"message\": \"The username '%s' already exists.\"\n"
                "  }\n"
                "}", username);
            response->content_length = body_len;
            return -1;
        }
        
        // Generic database error
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        response->status_code = HTTP_INTERNAL_SERVER_ERROR;
        strcpy(response->content_type, "application/json");
        int body_len = snprintf(response->body, MAX_JSON_SIZE, "{\"error\":{\"code\":\"Base.1.15.0.GeneralError\",\"message\":\"Failed to create account\"}}");
        response->content_length = body_len;
        return -1;
    }

    int last_id = account_id;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    response->status_code = HTTP_CREATED;  
    snprintf(response->content_type, sizeof(response->content_type), "application/json");
    snprintf(response->body, MAX_JSON_SIZE,
        "{\n"
        "  \"@odata.id\": \"/redfish/v1/AccountService/Accounts/%d\",\n"
        "  \"Id\": \"%d\",\n"
        "  \"UserName\": \"%s\",\n"
        "  \"RoleId\": \"%s\",\n"
        "  \"Enabled\": true\n"
        "}",
        last_id, last_id, username, role);

    response->content_length = strlen(response->body);
    
    // Add Location header for created resource
    if (response->header_count < MAX_HEADERS) {
        strcpy(response->headers[response->header_count][0], "Location");
        snprintf(response->headers[response->header_count][1], 256, "/redfish/v1/AccountService/Accounts/%d", last_id);
        response->header_count++;
    }

    return SUCCESS;
}


// Update ManagerAccount writable properties. Any argument that is NULL will be ignored.
// out_updated_password/out_updated_role are optional flags set to 1 if that column was updated.
int account_update(int account_id,
                   const char *new_password,
                   const char *new_role,
                   int *out_updated_password,
                   int *out_updated_role)
{
    if (out_updated_password) *out_updated_password = 0;
    if (out_updated_role) *out_updated_role = 0;

    if (account_id <= 0 || (!new_password && !new_role)) {
        return DB_STATUS_UNKNOW;
    }

    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return DB_STATUS_OPEN_ERROR;
    }

    // Ensure accounts table exists (defensive)
    const char *sql_create_table =
        "CREATE TABLE IF NOT EXISTS accounts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE NOT NULL,"
        "password TEXT NOT NULL,"
        "role TEXT NOT NULL"
        ");";
    char *err_msg = NULL;
    rc = sqlite3_exec(db, sql_create_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return DB_STATUS_PREPARE_ERROR;
    }

    // Build UPDATE based on provided fields
    const char *sql_update_pw_and_role = "UPDATE accounts SET password = ?, role = ? WHERE id = ?;";
    const char *sql_update_pw_only     = "UPDATE accounts SET password = ? WHERE id = ?;";
    const char *sql_update_role_only   = "UPDATE accounts SET role = ? WHERE id = ?;";

    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;

    if (new_password && new_role) {
        sql = sql_update_pw_and_role;
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare UPDATE statement: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return DB_STATUS_PREPARE_ERROR;
        }
        sqlite3_bind_text(stmt, 1, new_password, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, new_role, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, account_id);
    } else if (new_password) {
        sql = sql_update_pw_only;
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare UPDATE (password) statement: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return DB_STATUS_PREPARE_ERROR;
        }
        sqlite3_bind_text(stmt, 1, new_password, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, account_id);
    } else { // new_role only
        sql = sql_update_role_only;
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare UPDATE (role) statement: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return DB_STATUS_PREPARE_ERROR;
        }
        sqlite3_bind_text(stmt, 1, new_role, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, account_id);
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute UPDATE: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return DB_STATUS_SELECT_ERROR;
    }

    int changes = sqlite3_changes(db);
    if (changes > 0) {
        if (out_updated_password && new_password) *out_updated_password = 1;
        if (out_updated_role && new_role) *out_updated_role = 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return SUCCESS;
}

int account_delete(int account_id)
{
    if (account_id <= 0) {
        return DB_STATUS_UNKNOW;
    }

    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return DB_STATUS_OPEN_ERROR;
    }

    const char *sql_delete = "DELETE FROM accounts WHERE id = ?;";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_delete, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare DELETE account: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return DB_STATUS_PREPARE_ERROR;
    }

    sqlite3_bind_int(stmt, 1, account_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to delete account: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return DB_STATUS_SELECT_ERROR;
    }

    int rows_affected = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    if (rows_affected == 0) {
        return DB_STATUS_USERNAME_MISMATCH; // Account not found
    }
    
    return SUCCESS;
}

int account_get(void) 
{
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    printf("[%s] Open database successfully\n", __FUNCTION__);

    const char *sql_select = "SELECT id, username, password, role FROM accounts;";

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char *username = (const char*)sqlite3_column_text(stmt, 1);
        const char *password = (const char*)sqlite3_column_text(stmt, 2);
        const char *role = (const char*)sqlite3_column_text(stmt, 3);

        printf("id: %d, username: %s, password: %s, role: %s\n", id, username, password, role);
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute SELECT: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_finalize(stmt);
    
    sqlite3_close(db);
    return SUCCESS;
}

int account_list_get(account_info_t* account_info_list)
{
    if (!account_info_list) {
        fprintf(stderr, "account_info_list is NULL\n");
        return -1;
    }

    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql_select = "SELECT id, username, password, role, enabled, locked FROM accounts ORDER BY id;";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        // Get data from database
        int id = sqlite3_column_int(stmt, 0);
        const char *username = (const char*)sqlite3_column_text(stmt, 1);
        const char *password = (const char*)sqlite3_column_text(stmt, 2);
        const char *role = (const char*)sqlite3_column_text(stmt, 3);
        int enabled = sqlite3_column_int(stmt, 4);
        int locked = sqlite3_column_int(stmt, 5);

        // Populate account_info_t structure
        account_info_list[count].id = id;
        
        if (username) {
            strncpy(account_info_list[count].username, username, MAX_USERNAME_LENGTH - 1);
            account_info_list[count].username[MAX_USERNAME_LENGTH - 1] = '\0';
        } else {
            account_info_list[count].username[0] = '\0';
        }

        if (password) {
            strncpy(account_info_list[count].password, password, MAX_PASSWORD_LENGTH - 1);
            account_info_list[count].password[MAX_PASSWORD_LENGTH - 1] = '\0';
        } else {
            account_info_list[count].password[0] = '\0';
        }

        if (role) {
            strncpy(account_info_list[count].role, role, MAX_ROLE_LENGTH - 1);
            account_info_list[count].role[MAX_ROLE_LENGTH - 1] = '\0';
        } else {
            account_info_list[count].role[0] = '\0';
        }

        // Set enabled and locked from database
        account_info_list[count].enabled = (enabled != 0);
        account_info_list[count].locked = (locked != 0);
        
        // Set timestamps (you may want to add these to your database schema)
        strcpy(account_info_list[count].created_at, "N/A");
        strcpy(account_info_list[count].last_accessed, "N/A");

        count++;
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute SELECT: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    printf("[%s] Retrieved %d accounts from database\n", __FUNCTION__, count);
    return count; // Return number of accounts retrieved
}

int account_count_get(void)
{
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql_count = "SELECT COUNT(*) FROM accounts;";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_count, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare COUNT statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    int count = 0;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    } else {
        fprintf(stderr, "Failed to execute COUNT statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return count;
}

int account_set_enabled(int account_id, bool enabled)
{
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql_update = "UPDATE accounts SET enabled = ? WHERE id = ?;";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_update, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare UPDATE statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 2, account_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute UPDATE: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    int changes = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    if (changes == 0) {
        printf("No account found with ID %d\n", account_id);
        return -1;
    }

    printf("Account %d enabled status set to %s\n", account_id, enabled ? "true" : "false");
    return SUCCESS;
}

int account_set_locked(int account_id, bool locked)
{
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql_update = "UPDATE accounts SET locked = ? WHERE id = ?;";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_update, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare UPDATE statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, locked ? 1 : 0);
    sqlite3_bind_int(stmt, 2, account_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute UPDATE: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    int changes = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    if (changes == 0) {
        printf("No account found with ID %d\n", account_id);
        return -1;
    }

    printf("Account %d locked status set to %s\n", account_id, locked ? "true" : "false");
    return SUCCESS;
}

int account_get_by_id(int account_id, account_info_t* account)
{
    if (!account) {
        fprintf(stderr, "account parameter is NULL\n");
        return -1;
    }

    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql_select = "SELECT id, username, password, role, enabled, locked FROM accounts WHERE id = ?;";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, account_id);
    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        // Get data from database
        int id = sqlite3_column_int(stmt, 0);
        const char *username = (const char*)sqlite3_column_text(stmt, 1);
        const char *password = (const char*)sqlite3_column_text(stmt, 2);
        const char *role = (const char*)sqlite3_column_text(stmt, 3);
        int enabled = sqlite3_column_int(stmt, 4);
        int locked = sqlite3_column_int(stmt, 5);

        // Populate account_info_t structure
        account->id = id;
        
        if (username) {
            strncpy(account->username, username, MAX_USERNAME_LENGTH - 1);
            account->username[MAX_USERNAME_LENGTH - 1] = '\0';
        } else {
            account->username[0] = '\0';
        }

        if (password) {
            strncpy(account->password, password, MAX_PASSWORD_LENGTH - 1);
            account->password[MAX_PASSWORD_LENGTH - 1] = '\0';
        } else {
            account->password[0] = '\0';
        }

        if (role) {
            strncpy(account->role, role, MAX_ROLE_LENGTH - 1);
            account->role[MAX_ROLE_LENGTH - 1] = '\0';
        } else {
            account->role[0] = '\0';
        }

        // Set enabled and locked from database
        account->enabled = (enabled != 0);
        account->locked = (locked != 0);
        
        // Set timestamps
        strcpy(account->created_at, "N/A");
        strcpy(account->last_accessed, "N/A");

        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return SUCCESS;
    } else {
        printf("Account with ID %d not found\n", account_id);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }
}

db_status_type_t account_check(const char *username, const char *password)
{
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return DB_STATUS_OPEN_ERROR;
    }

    const char *sql_select = "SELECT password FROM accounts WHERE username = ?;";
    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return DB_STATUS_PREPARE_ERROR; 
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *password_in_db = (const char *)sqlite3_column_text(stmt, 0);

        if (password_in_db == NULL) {
            printf("Password in DB is NULL.\n");
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return DB_STATUS_PASSWORD_NULL;
        }

        if (strcmp(password_in_db, password) == 0) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return SUCCESS;
        } else {
            printf("Password does not match.\n");
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return DB_STATUS_PASSWORD_MISMATCH; 
        }

    } else if (rc == SQLITE_DONE) {
        printf("Username '%s' not found in database.\n", username);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return DB_STATUS_USERNAME_MISMATCH; 
    } else {
        fprintf(stderr, "Failed to execute SELECT statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return DB_STATUS_SELECT_ERROR; 
    }

    return DB_STATUS_UNKNOW;
}



int session_add(const char *username, char *token_out, int* session_id_out)
{
    int rc;

    char token[MAX_TOKEN_LENGTH];

    // Clean up expired sessions before creating new one
    cleanup_expired_sessions();
    
    rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return DB_STATUS_OPEN_ERROR;
    }

    // Generate token
    generate_secure_token(token, MAX_TOKEN_LENGTH);

    // Calculate expiry
    time_t now = time(NULL);
    time_t expiry_time = now + SESSION_EXPIRY_SECONDS;

    char expiry_str[20];
    strftime(expiry_str, sizeof(expiry_str), "%Y-%m-%d %H:%M:%S", localtime(&expiry_time));

    // Step 1: Find first available session ID starting from 1
    const char *sql_find_gap = "SELECT id FROM sessions ORDER BY id;";
    sqlite3_stmt *stmt_gap;
    rc = sqlite3_prepare_v2(db, sql_find_gap, -1, &stmt_gap, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT session IDs: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return DB_STATUS_PREPARE_ERROR;
    }

    int session_id = 1;
    while ((rc = sqlite3_step(stmt_gap)) == SQLITE_ROW) {
        int existing_id = sqlite3_column_int(stmt_gap, 0);
        if (existing_id == session_id) {
            session_id++; // This ID is taken, try next
        } else {
            // Found a gap, use this ID
            break;
        }
    }
    sqlite3_finalize(stmt_gap);

    // Step 2: Query role from accounts table
    const char *sql_get_role =
        "SELECT role FROM accounts WHERE username = ?;";
    sqlite3_stmt *stmt_role;
    rc = sqlite3_prepare_v2(db, sql_get_role, -1, &stmt_role, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT role: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return DB_STATUS_PREPARE_ERROR;
    }

    sqlite3_bind_text(stmt_role, 1, username, -1, SQLITE_TRANSIENT);

    char role[64] = {0}; // adjust size as needed
    rc = sqlite3_step(stmt_role);
    if (rc == SQLITE_ROW) {
        const unsigned char *role_text = sqlite3_column_text(stmt_role, 0);
        strncpy(role, (const char *)role_text, sizeof(role) - 1);
    } else {
        fprintf(stderr, "Role not found for user: %s\n", username);
        sqlite3_finalize(stmt_role);
        sqlite3_close(db);
        return -1;
    }
    sqlite3_finalize(stmt_role);

    // Step 3: Insert session with specific ID
    const char *sql_insert_session =
        "INSERT INTO sessions (id, token, username, role, expiry) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_insert_session, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare INSERT session: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return DB_STATUS_PREPARE_ERROR;
    }

    sqlite3_bind_int(stmt, 1, session_id);
    sqlite3_bind_text(stmt, 2, token, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, role, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, expiry_str, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert session: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    strncpy(token_out, token, MAX_TOKEN_LENGTH);
    *session_id_out = session_id;

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    printf("Session created: id=%d, username=%s, role=%s, token=%s, expiry=%s\n", 
           session_id, username, role, token, expiry_str);

    return SUCCESS;
}


int session_delete(const char *token)
{
    if (!token) {
        return ERROR_INVALID_PARAM;
    }

    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return DB_STATUS_OPEN_ERROR;
    }

    const char *sql_delete = "DELETE FROM sessions WHERE token = ?;";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_delete, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare DELETE session: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return DB_STATUS_PREPARE_ERROR;
    }

    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to delete session: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return DB_STATUS_SELECT_ERROR;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return SUCCESS;
}

int session_delete_by_id(int session_id)
{
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return DB_STATUS_OPEN_ERROR;
    }

    const char *sql_delete = "DELETE FROM sessions WHERE id = ?;";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_delete, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare DELETE session by ID: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return DB_STATUS_PREPARE_ERROR;
    }

    sqlite3_bind_int(stmt, 1, session_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to delete session by ID: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return DB_STATUS_SELECT_ERROR;
    }

    int rows_affected = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    if (rows_affected == 0) {
        return DB_STATUS_USERNAME_MISMATCH; // Session not found
    }
    
    return SUCCESS;
}


int redfish_get_uuid(char *uuid_out, size_t uuid_size)
{
    int rc;

    if (uuid_out == NULL || uuid_size < 37) { // UUID format: 36 chars + null terminator
        fprintf(stderr, "Invalid parameters for redfish_get_uuid\n");
        return -1;
    }

    rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // Ensure the UUID table exists
    const char *sql_create_uuid_table =
        "CREATE TABLE IF NOT EXISTS system_uuid ("
        "id INTEGER PRIMARY KEY CHECK (id = 1),"
        "uuid TEXT NOT NULL"
        ");";
    
    char *err_msg = NULL;
    rc = sqlite3_exec(db, sql_create_uuid_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create uuid table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    const char *sql_select = "SELECT uuid FROM system_uuid WHERE id = 1;";
    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT UUID statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *uuid_in_db = (const char *)sqlite3_column_text(stmt, 0);
        
        if (uuid_in_db == NULL) {
            fprintf(stderr, "UUID in DB is NULL\n");
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return -1;
        }

        strncpy(uuid_out, uuid_in_db, uuid_size - 1);
        uuid_out[uuid_size - 1] = '\0';

        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return SUCCESS;

    } else if (rc == SQLITE_DONE) {
        // No UUID found in database - generate one
        sqlite3_finalize(stmt);
        
        // Read UUID from /proc/sys/kernel/random/uuid
        FILE *uuid_file = fopen("/proc/sys/kernel/random/uuid", "r");
        if (uuid_file == NULL) {
            fprintf(stderr, "Failed to open /proc/sys/kernel/random/uuid\n");
            sqlite3_close(db);
            return -1;
        }
        
        char generated_uuid[37];
        if (fgets(generated_uuid, sizeof(generated_uuid), uuid_file) == NULL) {
            fprintf(stderr, "Failed to read UUID from /proc/sys/kernel/random/uuid\n");
            fclose(uuid_file);
            sqlite3_close(db);
            return -1;
        }
        fclose(uuid_file);
        
        // Remove newline if present
        size_t len = strlen(generated_uuid);
        if (len > 0 && generated_uuid[len - 1] == '\n') {
            generated_uuid[len - 1] = '\0';
        }
        
        // Store the generated UUID in database
        const char *sql_insert = "INSERT INTO system_uuid (id, uuid) VALUES (1, ?);";
        sqlite3_stmt *insert_stmt;
        
        rc = sqlite3_prepare_v2(db, sql_insert, -1, &insert_stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare INSERT UUID statement: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return -1;
        }
        
        sqlite3_bind_text(insert_stmt, 1, generated_uuid, -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(insert_stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Failed to insert generated UUID: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(insert_stmt);
            sqlite3_close(db);
            return -1;
        }
        
        sqlite3_finalize(insert_stmt);
        sqlite3_close(db);
        
        // Return the generated UUID
        strncpy(uuid_out, generated_uuid, uuid_size - 1);
        uuid_out[uuid_size - 1] = '\0';
        
        printf("Generated and stored new UUID: %s\n", generated_uuid);
        return SUCCESS;
        
    } else {
        fprintf(stderr, "Failed to execute SELECT UUID statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }
}


int redfish_set_uuid(const char *uuid)
{
    int rc;

    if (uuid == NULL) {
        fprintf(stderr, "Invalid UUID parameter\n");
        return -1;
    }

    // Validate UUID format: 8-4-4-4-12 characters (36 total) with hyphens
    if (strlen(uuid) != 36) {
        fprintf(stderr, "Invalid UUID length: expected 36 characters\n");
        return -1;
    }

    // Check format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    if (uuid[8] != '-' || uuid[13] != '-' || uuid[18] != '-' || uuid[23] != '-') {
        fprintf(stderr, "Invalid UUID format: expected format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n");
        return -1;
    }

    // Validate hex characters (excluding hyphens)
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue; // Skip hyphens
        }
        if (!((uuid[i] >= '0' && uuid[i] <= '9') || 
              (uuid[i] >= 'a' && uuid[i] <= 'f') || 
              (uuid[i] >= 'A' && uuid[i] <= 'F'))) {
            fprintf(stderr, "Invalid UUID character at position %d: '%c'\n", i, uuid[i]);
            return -1;
        }
    }

    rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // Ensure the UUID table exists
    const char *sql_create_uuid_table =
        "CREATE TABLE IF NOT EXISTS system_uuid ("
        "id INTEGER PRIMARY KEY CHECK (id = 1),"
        "uuid TEXT NOT NULL"
        ");";
    
    char *err_msg = NULL;
    rc = sqlite3_exec(db, sql_create_uuid_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create uuid table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    // Use INSERT OR REPLACE to handle both initial insert and updates
    const char *sql_insert = "INSERT OR REPLACE INTO system_uuid (id, uuid) VALUES (1, ?);";
    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare INSERT UUID statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert/update UUID: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    printf("UUID set successfully: %s\n", uuid);

    return SUCCESS;
}

static int ensure_system_private_key_table(sqlite3 *ldb)
{
    char *err = NULL;
    int rc = SQLITE_OK;
    const char *sql_create =
        "CREATE TABLE IF NOT EXISTS system_private_key ("
        "  id INTEGER PRIMARY KEY CHECK (id = 1),"
        "  pem TEXT NOT NULL"
        ");";
    rc = sqlite3_exec(ldb, sql_create, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to ensure system_private_key table: %s\n", err);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

int system_private_key_store_pem(const char *pem)
{
    if (!pem || pem[0] == '\0') return -1;
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    if (ensure_system_private_key_table(db) != 0) { sqlite3_close(db); return -1; }

    const char *sql = "INSERT OR REPLACE INTO system_private_key (id, pem) VALUES (1, ?);";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare UPSERT system_private_key: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, pem, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int system_private_key_load_pem(char *pem_out, size_t pem_out_size)
{
    if (!pem_out || pem_out_size == 0) return -1;
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    if (ensure_system_private_key_table(db) != 0) { sqlite3_close(db); return -1; }

    const char *sql = "SELECT pem FROM system_private_key WHERE id = 1;";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT system_private_key: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    rc = sqlite3_step(stmt);
    int ret = -1;
    if (rc == SQLITE_ROW) {
        const unsigned char *pem = sqlite3_column_text(stmt, 0);
        if (pem) {
            strncpy(pem_out, (const char *)pem, pem_out_size - 1);
            pem_out[pem_out_size - 1] = '\0';
            ret = (int)strlen(pem_out);
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    if (ret < 0) {
        return -1;
    }
    return 0;
}

static int ensure_system_certificate_table(sqlite3 *db)
{
    char *err = NULL;
    const char *sql = "CREATE TABLE IF NOT EXISTS system_certificate (id INTEGER PRIMARY KEY, pem TEXT);";
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to ensure system_certificate table: %s\n", err);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

int system_certificate_store_pem(const char *pem)
{
    if (!pem || pem[0] == '\0') return -1;
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    if (ensure_system_certificate_table(db) != 0) { sqlite3_close(db); return -1; }

    const char *sql = "INSERT OR REPLACE INTO system_certificate (id, pem) VALUES (1, ?);";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare UPSERT system_certificate: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, pem, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int system_certificate_load_pem(char *pem_out, size_t pem_out_size)
{
    if (!pem_out || pem_out_size == 0) return -1;
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    if (ensure_system_certificate_table(db) != 0) { sqlite3_close(db); return -1; }

    const char *sql = "SELECT pem FROM system_certificate WHERE id = 1;";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT system_certificate: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    rc = sqlite3_step(stmt);
    int ret = -1;
    if (rc == SQLITE_ROW) {
        const unsigned char *pem = sqlite3_column_text(stmt, 0);
        if (pem) {
            strncpy(pem_out, (const char *)pem, pem_out_size - 1);
            pem_out[pem_out_size - 1] = '\0';
            ret = (int)strlen(pem_out);
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    if (ret < 0) {
        return -1;
    }
    return 0;
}

static int ensure_system_root_certificate_table(sqlite3 *db)
{
    char *err = NULL;
    const char *sql = "CREATE TABLE IF NOT EXISTS system_root_certificate (id INTEGER PRIMARY KEY, pem TEXT);";
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to ensure system_root_certificate table: %s\n", err);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

int system_root_certificate_store_pem(const char *pem)
{
    if (!pem || pem[0] == '\0') return -1;
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    if (ensure_system_root_certificate_table(db) != 0) { sqlite3_close(db); return -1; }

    const char *sql = "INSERT OR REPLACE INTO system_root_certificate (id, pem) VALUES (1, ?);";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare UPSERT system_root_certificate: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, pem, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int system_root_certificate_load_pem(char *pem_out, size_t pem_out_size)
{
    if (!pem_out || pem_out_size == 0) return -1;
    int rc = sqlite3_open(CONFIG_REDFISH_ACCOUNT_DB_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    if (ensure_system_root_certificate_table(db) != 0) { sqlite3_close(db); return -1; }

    const char *sql = "SELECT pem FROM system_root_certificate WHERE id = 1;";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare SELECT system_root_certificate: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    rc = sqlite3_step(stmt);
    int ret = -1;
    if (rc == SQLITE_ROW) {
        const unsigned char *pem = sqlite3_column_text(stmt, 0);
        if (pem) {
            strncpy(pem_out, (const char *)pem, pem_out_size - 1);
            pem_out[pem_out_size - 1] = '\0';
            ret = (int)strlen(pem_out);
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    if (ret < 0) {
        return -1;
    }
    return 0;
}


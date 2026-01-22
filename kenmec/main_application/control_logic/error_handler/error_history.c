/**
 * @file error_history.c
 * @brief 錯誤碼歷史記錄模組實作
 */

#include "error_history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

/* ========== 私有變數 ========== */

/** 歷史記錄檔案路徑 */
static char g_history_path[256] = ERROR_HISTORY_DEFAULT_PATH;

/** 歷史記錄快取 */
static error_history_record_t g_records[ERROR_HISTORY_MAX_RECORDS];

/** 當前記錄數量 */
static uint32_t g_record_count = 0;

/** 統計資訊快取 */
static error_history_stats_t g_stats[ERROR_HANDLER_MAX_ERRORS];
static uint32_t g_stats_count = 0;

/** 保護的互斥鎖 */
static pthread_mutex_t g_history_mutex = PTHREAD_MUTEX_INITIALIZER;

/** 初始化標記 */
static bool g_initialized = false;

/** 髒資料標記 (需要寫入檔案) */
static bool g_dirty = false;

/* ========== 私有函數宣告 ========== */

static int ensure_directory_exists(const char *path);
static int load_from_file(void);
static int save_to_file(void);
static int find_stats_index(uint32_t code);
static void update_statistics(const error_history_record_t *record);

/* ========== 私有函數實作 ========== */

/**
 * @brief 確保目錄存在
 */
static int ensure_directory_exists(const char *path) {
    char dir_path[256];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    /* 找到最後一個 / 的位置 */
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash == NULL) {
        return 0;  /* 沒有目錄部分 */
    }

    *last_slash = '\0';

    /* 逐層建立目錄 */
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir_path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

/**
 * @brief 從檔案載入歷史記錄
 */
static int load_from_file(void) {
    FILE *fp = fopen(g_history_path, "r");
    if (fp == NULL) {
        /* 檔案不存在是正常的 */
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(fp);
        return 0;
    }

    char *json_str = (char *)malloc(file_size + 1);
    if (json_str == NULL) {
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(json_str, 1, file_size, fp);
    json_str[read_size] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (root == NULL) {
        printf("[ERROR_HISTORY] Failed to parse history file\n");
        return -2;
    }

    /* 解析記錄陣列 */
    cJSON *records = cJSON_GetObjectItem(root, "records");
    if (records != NULL && cJSON_IsArray(records)) {
        cJSON *record_item;
        cJSON_ArrayForEach(record_item, records) {
            if (g_record_count >= ERROR_HISTORY_MAX_RECORDS) {
                break;
            }

            error_history_record_t *rec = &g_records[g_record_count];

            cJSON *code = cJSON_GetObjectItem(record_item, "code");
            if (code != NULL) rec->code = (uint32_t)code->valueint;

            cJSON *name = cJSON_GetObjectItem(record_item, "name");
            if (name != NULL && name->valuestring != NULL) {
                strncpy(rec->name, name->valuestring, sizeof(rec->name) - 1);
            }

            cJSON *timestamp = cJSON_GetObjectItem(record_item, "timestamp");
            if (timestamp != NULL) rec->timestamp = (uint32_t)timestamp->valueint;

            cJSON *value = cJSON_GetObjectItem(record_item, "value");
            if (value != NULL) rec->value = (float)value->valuedouble;

            cJSON *threshold = cJSON_GetObjectItem(record_item, "threshold");
            if (threshold != NULL) rec->threshold = (float)threshold->valuedouble;

            cJSON *severity = cJSON_GetObjectItem(record_item, "severity");
            if (severity != NULL) rec->severity = (error_severity_t)severity->valueint;

            cJSON *duration = cJSON_GetObjectItem(record_item, "duration_sec");
            if (duration != NULL) rec->duration_sec = (uint32_t)duration->valueint;

            cJSON *recovered = cJSON_GetObjectItem(record_item, "recovered");
            if (recovered != NULL) rec->recovered = cJSON_IsTrue(recovered);

            /* 更新統計 */
            update_statistics(rec);

            g_record_count++;
        }
    }

    cJSON_Delete(root);
    printf("[ERROR_HISTORY] Loaded %u records from file\n", g_record_count);
    return 0;
}

/**
 * @brief 儲存歷史記錄到檔案
 */
static int save_to_file(void) {
    if (!g_dirty) {
        return 0;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", ERROR_HISTORY_VERSION);
    cJSON_AddNumberToObject(root, "record_count", g_record_count);

    cJSON *records = cJSON_CreateArray();
    for (uint32_t i = 0; i < g_record_count; i++) {
        cJSON *record = cJSON_CreateObject();
        cJSON_AddNumberToObject(record, "code", g_records[i].code);
        cJSON_AddStringToObject(record, "name", g_records[i].name);
        cJSON_AddNumberToObject(record, "timestamp", g_records[i].timestamp);
        cJSON_AddNumberToObject(record, "value", g_records[i].value);
        cJSON_AddNumberToObject(record, "threshold", g_records[i].threshold);
        cJSON_AddNumberToObject(record, "severity", g_records[i].severity);
        cJSON_AddNumberToObject(record, "duration_sec", g_records[i].duration_sec);
        cJSON_AddBoolToObject(record, "recovered", g_records[i].recovered);
        cJSON_AddItemToArray(records, record);
    }
    cJSON_AddItemToObject(root, "records", records);

    /* 儲存統計資訊 */
    cJSON *stats = cJSON_CreateObject();
    for (uint32_t i = 0; i < g_stats_count; i++) {
        char code_str[16];
        snprintf(code_str, sizeof(code_str), "%u", g_stats[i].code);

        cJSON *stat = cJSON_CreateObject();
        cJSON_AddNumberToObject(stat, "total_count", g_stats[i].total_count);
        cJSON_AddNumberToObject(stat, "last_timestamp", g_stats[i].last_timestamp);
        cJSON_AddNumberToObject(stat, "total_duration", g_stats[i].total_duration);
        cJSON_AddNumberToObject(stat, "max_value", g_stats[i].max_value);
        cJSON_AddItemToObject(stats, code_str, stat);
    }
    cJSON_AddItemToObject(root, "statistics", stats);

    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        return -1;
    }

    FILE *fp = fopen(g_history_path, "w");
    if (fp == NULL) {
        free(json_str);
        printf("[ERROR_HISTORY] Failed to open file for writing: %s\n", g_history_path);
        return -2;
    }

    fprintf(fp, "%s", json_str);
    fclose(fp);
    free(json_str);

    g_dirty = false;
    printf("[ERROR_HISTORY] Saved %u records to file\n", g_record_count);
    return 0;
}

/**
 * @brief 查找統計資訊索引
 */
static int find_stats_index(uint32_t code) {
    for (uint32_t i = 0; i < g_stats_count; i++) {
        if (g_stats[i].code == code) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief 更新統計資訊
 */
static void update_statistics(const error_history_record_t *record) {
    int idx = find_stats_index(record->code);

    if (idx < 0) {
        /* 新增統計條目 */
        if (g_stats_count >= ERROR_HANDLER_MAX_ERRORS) {
            return;
        }
        idx = g_stats_count++;
        g_stats[idx].code = record->code;
        g_stats[idx].total_count = 0;
        g_stats[idx].last_timestamp = 0;
        g_stats[idx].total_duration = 0;
        g_stats[idx].max_value = 0;
    }

    g_stats[idx].total_count++;
    g_stats[idx].last_timestamp = record->timestamp;
    g_stats[idx].total_duration += record->duration_sec;
    if (record->value > g_stats[idx].max_value) {
        g_stats[idx].max_value = record->value;
    }
}

/* ========== API 函數實作 ========== */

int error_history_init(const char *log_path) {
    pthread_mutex_lock(&g_history_mutex);

    if (log_path != NULL) {
        strncpy(g_history_path, log_path, sizeof(g_history_path) - 1);
        g_history_path[sizeof(g_history_path) - 1] = '\0';
    }

    /* 確保目錄存在 */
    if (ensure_directory_exists(g_history_path) != 0) {
        printf("[ERROR_HISTORY] Warning: Failed to create directory\n");
    }

    /* 重置狀態 */
    g_record_count = 0;
    g_stats_count = 0;
    g_dirty = false;
    memset(g_records, 0, sizeof(g_records));
    memset(g_stats, 0, sizeof(g_stats));

    /* 從檔案載入現有記錄 */
    load_from_file();

    g_initialized = true;

    pthread_mutex_unlock(&g_history_mutex);

    printf("[ERROR_HISTORY] Initialized with path: %s\n", g_history_path);
    return 0;
}

void error_history_cleanup(void) {
    pthread_mutex_lock(&g_history_mutex);

    /* 儲存未保存的記錄 */
    save_to_file();

    g_initialized = false;
    g_record_count = 0;
    g_stats_count = 0;

    pthread_mutex_unlock(&g_history_mutex);

    printf("[ERROR_HISTORY] Cleanup completed\n");
}

int error_history_log(const error_runtime_t *runtime, const error_definition_t *def) {
    if (runtime == NULL || def == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_history_mutex);

    if (!g_initialized) {
        pthread_mutex_unlock(&g_history_mutex);
        return -2;
    }

    /* 如果已滿，移除最舊的記錄 */
    if (g_record_count >= ERROR_HISTORY_MAX_RECORDS) {
        memmove(&g_records[0], &g_records[1],
                sizeof(error_history_record_t) * (ERROR_HISTORY_MAX_RECORDS - 1));
        g_record_count--;
    }

    /* 新增記錄 */
    error_history_record_t *rec = &g_records[g_record_count];
    rec->code = runtime->code;
    strncpy(rec->name, def->name, sizeof(rec->name) - 1);
    rec->timestamp = runtime->trigger_timestamp;
    rec->value = runtime->last_value;
    rec->threshold = def->threshold;
    rec->severity = def->severity;
    rec->duration_sec = 0;
    rec->recovered = false;

    g_record_count++;
    g_dirty = true;

    /* 更新統計 */
    update_statistics(rec);

    pthread_mutex_unlock(&g_history_mutex);

    printf("[ERROR_HISTORY] Logged error code %u: %s\n", runtime->code, def->name);
    return 0;
}

int error_history_log_recovery(uint32_t code) {
    pthread_mutex_lock(&g_history_mutex);

    /* 找到最新的未恢復記錄 */
    for (int i = g_record_count - 1; i >= 0; i--) {
        if (g_records[i].code == code && !g_records[i].recovered) {
            g_records[i].recovered = true;
            g_records[i].duration_sec = (uint32_t)time(NULL) - g_records[i].timestamp;
            g_dirty = true;

            /* 更新統計中的持續時間 */
            int idx = find_stats_index(code);
            if (idx >= 0) {
                g_stats[idx].total_duration += g_records[i].duration_sec;
            }

            pthread_mutex_unlock(&g_history_mutex);
            printf("[ERROR_HISTORY] Logged recovery for code %u, duration: %u sec\n",
                   code, g_records[i].duration_sec);
            return 0;
        }
    }

    pthread_mutex_unlock(&g_history_mutex);
    return -1;
}

int error_history_get_recent(uint32_t count, cJSON *json_out) {
    if (json_out == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_history_mutex);

    cJSON *records = cJSON_CreateArray();

    uint32_t start = (g_record_count > count) ? (g_record_count - count) : 0;
    for (uint32_t i = g_record_count; i > start; i--) {
        uint32_t idx = i - 1;
        cJSON *record = cJSON_CreateObject();
        cJSON_AddNumberToObject(record, "code", g_records[idx].code);
        cJSON_AddStringToObject(record, "name", g_records[idx].name);
        cJSON_AddNumberToObject(record, "timestamp", g_records[idx].timestamp);
        cJSON_AddNumberToObject(record, "value", g_records[idx].value);
        cJSON_AddNumberToObject(record, "threshold", g_records[idx].threshold);
        cJSON_AddStringToObject(record, "severity",
                                error_severity_to_string(g_records[idx].severity));
        cJSON_AddNumberToObject(record, "duration_sec", g_records[idx].duration_sec);
        cJSON_AddBoolToObject(record, "recovered", g_records[idx].recovered);

        /* 添加可讀時間格式 */
        char time_str[32];
        struct tm *tm_info = localtime((time_t *)&g_records[idx].timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm_info);
        cJSON_AddStringToObject(record, "timestamp_str", time_str);

        cJSON_AddItemToArray(records, record);
    }

    cJSON_AddItemToObject(json_out, "records", records);
    cJSON_AddNumberToObject(json_out, "total_count", g_record_count);

    pthread_mutex_unlock(&g_history_mutex);
    return 0;
}

int error_history_get_by_code(uint32_t code, uint32_t count, cJSON *json_out) {
    if (json_out == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_history_mutex);

    cJSON *records = cJSON_CreateArray();
    uint32_t found = 0;

    for (int i = g_record_count - 1; i >= 0 && found < count; i--) {
        if (g_records[i].code == code) {
            cJSON *record = cJSON_CreateObject();
            cJSON_AddNumberToObject(record, "timestamp", g_records[i].timestamp);
            cJSON_AddNumberToObject(record, "value", g_records[i].value);
            cJSON_AddNumberToObject(record, "duration_sec", g_records[i].duration_sec);
            cJSON_AddBoolToObject(record, "recovered", g_records[i].recovered);
            cJSON_AddItemToArray(records, record);
            found++;
        }
    }

    cJSON_AddItemToObject(json_out, "records", records);
    cJSON_AddNumberToObject(json_out, "code", code);

    pthread_mutex_unlock(&g_history_mutex);
    return 0;
}

int error_history_get_statistics(uint32_t code, cJSON *json_out) {
    if (json_out == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_history_mutex);

    if (code == 0) {
        /* 返回所有錯誤碼的統計 */
        cJSON *stats = cJSON_CreateArray();
        for (uint32_t i = 0; i < g_stats_count; i++) {
            cJSON *stat = cJSON_CreateObject();
            cJSON_AddNumberToObject(stat, "code", g_stats[i].code);
            cJSON_AddNumberToObject(stat, "total_count", g_stats[i].total_count);
            cJSON_AddNumberToObject(stat, "last_timestamp", g_stats[i].last_timestamp);
            cJSON_AddNumberToObject(stat, "total_duration", g_stats[i].total_duration);
            cJSON_AddNumberToObject(stat, "max_value", g_stats[i].max_value);
            cJSON_AddItemToArray(stats, stat);
        }
        cJSON_AddItemToObject(json_out, "statistics", stats);
    } else {
        /* 返回特定錯誤碼的統計 */
        int idx = find_stats_index(code);
        if (idx >= 0) {
            cJSON_AddNumberToObject(json_out, "code", g_stats[idx].code);
            cJSON_AddNumberToObject(json_out, "total_count", g_stats[idx].total_count);
            cJSON_AddNumberToObject(json_out, "last_timestamp", g_stats[idx].last_timestamp);
            cJSON_AddNumberToObject(json_out, "total_duration", g_stats[idx].total_duration);
            cJSON_AddNumberToObject(json_out, "max_value", g_stats[idx].max_value);
        } else {
            cJSON_AddNumberToObject(json_out, "code", code);
            cJSON_AddNumberToObject(json_out, "total_count", 0);
        }
    }

    pthread_mutex_unlock(&g_history_mutex);
    return 0;
}

int error_history_clear(void) {
    pthread_mutex_lock(&g_history_mutex);

    g_record_count = 0;
    g_stats_count = 0;
    g_dirty = true;
    memset(g_records, 0, sizeof(g_records));
    memset(g_stats, 0, sizeof(g_stats));

    save_to_file();

    pthread_mutex_unlock(&g_history_mutex);

    printf("[ERROR_HISTORY] All records cleared\n");
    return 0;
}

int error_history_clear_before(uint32_t before_timestamp) {
    pthread_mutex_lock(&g_history_mutex);

    uint32_t new_count = 0;
    uint32_t cleared = 0;

    for (uint32_t i = 0; i < g_record_count; i++) {
        if (g_records[i].timestamp >= before_timestamp) {
            if (new_count != i) {
                memcpy(&g_records[new_count], &g_records[i], sizeof(error_history_record_t));
            }
            new_count++;
        } else {
            cleared++;
        }
    }

    g_record_count = new_count;
    if (cleared > 0) {
        g_dirty = true;
    }

    pthread_mutex_unlock(&g_history_mutex);

    printf("[ERROR_HISTORY] Cleared %u records before timestamp %u\n", cleared, before_timestamp);
    return (int)cleared;
}

int error_history_flush(void) {
    pthread_mutex_lock(&g_history_mutex);
    int result = save_to_file();
    pthread_mutex_unlock(&g_history_mutex);
    return result;
}

uint32_t error_history_get_count(void) {
    pthread_mutex_lock(&g_history_mutex);
    uint32_t count = g_record_count;
    pthread_mutex_unlock(&g_history_mutex);
    return count;
}

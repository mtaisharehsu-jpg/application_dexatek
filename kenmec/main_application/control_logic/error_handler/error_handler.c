/**
 * @file error_handler.c
 * @brief 通用錯誤處理框架實作
 */

#include "error_handler.h"
#include "error_history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>

/* ========== 私有變數 ========== */

/** 錯誤處理器上下文 (全域單例) */
static error_handler_context_t g_error_ctx = {0};

/** 保護上下文的互斥鎖 */
static pthread_mutex_t g_error_mutex = PTHREAD_MUTEX_INITIALIZER;

/** 狀態變更回調函數 */
static error_state_callback_t g_state_callback = NULL;

/* ========== 私有函數宣告 ========== */

static int find_error_index(uint32_t code);
static bool check_condition(const error_definition_t *def, float *current_value);
static bool check_reset_condition(const error_definition_t *def, float current_value);
static void update_error_state(uint32_t index, error_state_t new_state);
static void invoke_action_handler(uint32_t index, error_state_t new_state);

/* ========== 輔助函數實作 ========== */

const char *error_severity_to_string(error_severity_t severity) {
    switch (severity) {
        case ERROR_SEVERITY_MINOR:    return "minor";
        case ERROR_SEVERITY_MAJOR:    return "major";
        case ERROR_SEVERITY_CRITICAL: return "critical";
        default:                      return "unknown";
    }
}

const char *error_state_to_string(error_state_t state) {
    switch (state) {
        case ERROR_STATE_INACTIVE:     return "inactive";
        case ERROR_STATE_PENDING:      return "pending";
        case ERROR_STATE_ACTIVE:       return "active";
        case ERROR_STATE_ACKNOWLEDGED: return "acknowledged";
        default:                       return "unknown";
    }
}

uint32_t error_handler_get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
}

static uint32_t get_unix_timestamp(void) {
    return (uint32_t)time(NULL);
}

/* ========== 私有函數實作 ========== */

/**
 * @brief 根據錯誤碼查找索引
 */
static int find_error_index(uint32_t code) {
    for (uint32_t i = 0; i < g_error_ctx.registered_count; i++) {
        if (g_error_ctx.definitions[i].code == code) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief 檢查錯誤條件是否滿足
 */
static bool check_condition(const error_definition_t *def, float *current_value) {
    float value = 0.0f;

    /* 獲取當前值 */
    if (def->get_value != NULL) {
        value = def->get_value();
    }

    if (current_value != NULL) {
        *current_value = value;
    }

    /* 根據條件類型檢查 */
    switch (def->cond_type) {
        case ERROR_COND_THRESHOLD_HIGH:
            return (value > def->threshold);

        case ERROR_COND_THRESHOLD_LOW:
            return (value < def->threshold);

        case ERROR_COND_EQUAL:
            return (value == def->threshold);

        case ERROR_COND_NOT_EQUAL:
            return (value != def->threshold);

        case ERROR_COND_CUSTOM:
            if (def->condition_check != NULL) {
                return (def->condition_check() != 0);
            }
            return false;

        default:
            return false;
    }
}

/**
 * @brief 檢查重置條件是否滿足 (考慮遲滯)
 */
static bool check_reset_condition(const error_definition_t *def, float current_value) {
    switch (def->cond_type) {
        case ERROR_COND_THRESHOLD_HIGH:
            /* 高閾值觸發的錯誤，需低於 (閾值 - 遲滯) 才重置 */
            return (current_value < (def->threshold - def->hysteresis));

        case ERROR_COND_THRESHOLD_LOW:
            /* 低閾值觸發的錯誤，需高於 (閾值 + 遲滯) 才重置 */
            return (current_value > (def->threshold + def->hysteresis));

        case ERROR_COND_EQUAL:
            return (current_value != def->threshold);

        case ERROR_COND_NOT_EQUAL:
            return (current_value == def->threshold);

        case ERROR_COND_CUSTOM:
            if (def->condition_check != NULL) {
                return (def->condition_check() == 0);
            }
            return true;

        default:
            return true;
    }
}

/**
 * @brief 更新錯誤狀態並觸發回調
 */
static void update_error_state(uint32_t index, error_state_t new_state) {
    error_runtime_t *rt = &g_error_ctx.runtimes[index];
    error_state_t old_state = rt->state;

    if (old_state == new_state) {
        return;
    }

    rt->state = new_state;
    rt->modbus_synced = false;

    /* 更新活動錯誤計數 */
    if (new_state == ERROR_STATE_ACTIVE && old_state != ERROR_STATE_ACTIVE) {
        g_error_ctx.active_count++;
        g_error_ctx.latest_error_code = rt->code;
        rt->trigger_timestamp = get_unix_timestamp();
        rt->occurrence_count++;

        /* 記錄到歷史 */
        if (g_error_ctx.history_enabled) {
            error_history_log(rt, &g_error_ctx.definitions[index]);
        }
    } else if (new_state == ERROR_STATE_INACTIVE && old_state != ERROR_STATE_INACTIVE) {
        if (g_error_ctx.active_count > 0) {
            g_error_ctx.active_count--;
        }
    }

    /* 觸發狀態變更回調 */
    if (g_state_callback != NULL) {
        g_state_callback(rt->code, old_state, new_state);
    }

    /* 觸發動作處理器 */
    invoke_action_handler(index, new_state);

    printf("[ERROR_HANDLER] Code %u (%s): %s -> %s\n",
           rt->code,
           g_error_ctx.definitions[index].name,
           error_state_to_string(old_state),
           error_state_to_string(new_state));
}

/**
 * @brief 呼叫錯誤動作處理器
 */
static void invoke_action_handler(uint32_t index, error_state_t new_state) {
    const error_definition_t *def = &g_error_ctx.definitions[index];
    if (def->action_handler != NULL) {
        def->action_handler(def->code, new_state);
    }
}

/* ========== API 函數實作 ========== */

int error_handler_init(void) {
    pthread_mutex_lock(&g_error_mutex);

    memset(&g_error_ctx, 0, sizeof(g_error_ctx));
    g_error_ctx.initialized = true;
    g_error_ctx.modbus_enabled = true;
    g_error_ctx.history_enabled = true;

    /* 初始化歷史記錄模組 */
    error_history_init(NULL);

    pthread_mutex_unlock(&g_error_mutex);

    printf("[ERROR_HANDLER] Initialized successfully\n");
    return 0;
}

void error_handler_cleanup(void) {
    pthread_mutex_lock(&g_error_mutex);

    error_history_cleanup();
    memset(&g_error_ctx, 0, sizeof(g_error_ctx));

    pthread_mutex_unlock(&g_error_mutex);

    printf("[ERROR_HANDLER] Cleanup completed\n");
}

int error_handler_reset_all(void) {
    pthread_mutex_lock(&g_error_mutex);

    for (uint32_t i = 0; i < g_error_ctx.registered_count; i++) {
        g_error_ctx.runtimes[i].state = ERROR_STATE_INACTIVE;
        g_error_ctx.runtimes[i].pending_start_ms = 0;
        g_error_ctx.runtimes[i].modbus_synced = false;
    }
    g_error_ctx.active_count = 0;
    g_error_ctx.latest_error_code = 0;

    pthread_mutex_unlock(&g_error_mutex);

    printf("[ERROR_HANDLER] All errors reset\n");
    return 0;
}

int error_handler_register(const error_definition_t *def) {
    if (def == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_error_mutex);

    if (g_error_ctx.registered_count >= ERROR_HANDLER_MAX_ERRORS) {
        pthread_mutex_unlock(&g_error_mutex);
        printf("[ERROR_HANDLER] Error: Maximum error count reached\n");
        return -2;
    }

    /* 檢查是否已存在 */
    if (find_error_index(def->code) >= 0) {
        pthread_mutex_unlock(&g_error_mutex);
        printf("[ERROR_HANDLER] Error: Code %u already registered\n", def->code);
        return -3;
    }

    /* 複製定義 */
    uint32_t idx = g_error_ctx.registered_count;
    memcpy(&g_error_ctx.definitions[idx], def, sizeof(error_definition_t));

    /* 初始化運行時狀態 */
    g_error_ctx.runtimes[idx].code = def->code;
    g_error_ctx.runtimes[idx].state = ERROR_STATE_INACTIVE;
    g_error_ctx.runtimes[idx].pending_start_ms = 0;
    g_error_ctx.runtimes[idx].trigger_timestamp = 0;
    g_error_ctx.runtimes[idx].occurrence_count = 0;
    g_error_ctx.runtimes[idx].last_value = 0.0f;
    g_error_ctx.runtimes[idx].modbus_synced = false;

    g_error_ctx.registered_count++;

    pthread_mutex_unlock(&g_error_mutex);

    printf("[ERROR_HANDLER] Registered error code %u: %s\n", def->code, def->name);
    return 0;
}

int error_handler_register_batch(const error_definition_t *defs, uint32_t count) {
    if (defs == NULL || count == 0) {
        return -1;
    }

    int result = 0;
    for (uint32_t i = 0; i < count; i++) {
        int ret = error_handler_register(&defs[i]);
        if (ret != 0) {
            printf("[ERROR_HANDLER] Failed to register error code %u\n", defs[i].code);
            result = ret;
        }
    }

    return result;
}

int error_handler_load_config(const char *config_path) {
    if (config_path == NULL) {
        return -1;
    }

    /* 讀取 JSON 配置檔 */
    FILE *fp = fopen(config_path, "r");
    if (fp == NULL) {
        printf("[ERROR_HANDLER] Failed to open config file: %s\n", config_path);
        return -2;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *json_str = (char *)malloc(file_size + 1);
    if (json_str == NULL) {
        fclose(fp);
        return -3;
    }

    size_t read_size = fread(json_str, 1, file_size, fp);
    json_str[read_size] = '\0';
    fclose(fp);

    /* 解析 JSON */
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (root == NULL) {
        printf("[ERROR_HANDLER] Failed to parse config JSON\n");
        return -4;
    }

    /* 解析錯誤碼陣列 */
    cJSON *errors = cJSON_GetObjectItem(root, "errors");
    if (errors == NULL || !cJSON_IsArray(errors)) {
        cJSON_Delete(root);
        return -5;
    }

    pthread_mutex_lock(&g_error_mutex);

    cJSON *error_item;
    cJSON_ArrayForEach(error_item, errors) {
        cJSON *code_json = cJSON_GetObjectItem(error_item, "code");
        if (code_json == NULL) continue;

        uint32_t code = (uint32_t)code_json->valueint;
        int idx = find_error_index(code);
        if (idx < 0) continue;

        /* 更新可配置參數 */
        cJSON *enabled = cJSON_GetObjectItem(error_item, "enabled");
        if (enabled != NULL && cJSON_IsBool(enabled)) {
            g_error_ctx.definitions[idx].enabled = cJSON_IsTrue(enabled);
        }

        cJSON *threshold = cJSON_GetObjectItem(error_item, "threshold");
        if (threshold != NULL && cJSON_IsNumber(threshold)) {
            g_error_ctx.definitions[idx].threshold = (float)threshold->valuedouble;
        }

        cJSON *hysteresis = cJSON_GetObjectItem(error_item, "hysteresis");
        if (hysteresis != NULL && cJSON_IsNumber(hysteresis)) {
            g_error_ctx.definitions[idx].hysteresis = (float)hysteresis->valuedouble;
        }

        cJSON *delay_ms = cJSON_GetObjectItem(error_item, "delay_ms");
        if (delay_ms != NULL && cJSON_IsNumber(delay_ms)) {
            g_error_ctx.definitions[idx].delay_ms = (uint32_t)delay_ms->valueint;
        }

        printf("[ERROR_HANDLER] Updated config for code %u\n", code);
    }

    pthread_mutex_unlock(&g_error_mutex);
    cJSON_Delete(root);

    printf("[ERROR_HANDLER] Config loaded from: %s\n", config_path);
    return 0;
}

int error_handler_process(void) {
    pthread_mutex_lock(&g_error_mutex);

    uint32_t current_ms = error_handler_get_timestamp_ms();

    for (uint32_t i = 0; i < g_error_ctx.registered_count; i++) {
        error_definition_t *def = &g_error_ctx.definitions[i];
        error_runtime_t *rt = &g_error_ctx.runtimes[i];

        /* 跳過已停用的錯誤碼 */
        if (!def->enabled) {
            continue;
        }

        float current_value = 0.0f;
        bool condition_met = check_condition(def, &current_value);
        rt->last_value = current_value;

        switch (rt->state) {
            case ERROR_STATE_INACTIVE:
                if (condition_met) {
                    if (def->delay_ms > 0) {
                        /* 進入等待延遲狀態 */
                        rt->pending_start_ms = current_ms;
                        update_error_state(i, ERROR_STATE_PENDING);
                    } else {
                        /* 無延遲，直接觸發 */
                        update_error_state(i, ERROR_STATE_ACTIVE);
                    }
                }
                break;

            case ERROR_STATE_PENDING:
                if (!condition_met) {
                    /* 條件消失，回到正常 */
                    update_error_state(i, ERROR_STATE_INACTIVE);
                } else if ((current_ms - rt->pending_start_ms) >= def->delay_ms) {
                    /* 延遲時間到，觸發錯誤 */
                    update_error_state(i, ERROR_STATE_ACTIVE);
                }
                break;

            case ERROR_STATE_ACTIVE:
                if (def->recovery == ERROR_RECOVERY_AUTO) {
                    /* 自動復歸模式：條件消失且滿足遲滯則復歸 */
                    if (check_reset_condition(def, current_value)) {
                        update_error_state(i, ERROR_STATE_INACTIVE);
                    }
                }
                /* 手動復歸模式：需等待 acknowledge 後才能進入 ACKNOWLEDGED */
                break;

            case ERROR_STATE_ACKNOWLEDGED:
                /* 已確認，等待條件消失後復歸 */
                if (check_reset_condition(def, current_value)) {
                    update_error_state(i, ERROR_STATE_INACTIVE);
                }
                break;
        }
    }

    pthread_mutex_unlock(&g_error_mutex);

    /* 同步到 Modbus */
    if (g_error_ctx.modbus_enabled) {
        error_handler_sync_to_modbus();
    }

    return 0;
}

int error_handler_set_error(uint32_t code, bool active) {
    pthread_mutex_lock(&g_error_mutex);

    int idx = find_error_index(code);
    if (idx < 0) {
        pthread_mutex_unlock(&g_error_mutex);
        return -1;
    }

    if (active) {
        if (g_error_ctx.runtimes[idx].state == ERROR_STATE_INACTIVE) {
            update_error_state(idx, ERROR_STATE_ACTIVE);
        }
    } else {
        update_error_state(idx, ERROR_STATE_INACTIVE);
    }

    pthread_mutex_unlock(&g_error_mutex);
    return 0;
}

int error_handler_acknowledge(uint32_t code) {
    pthread_mutex_lock(&g_error_mutex);

    int idx = find_error_index(code);
    if (idx < 0) {
        pthread_mutex_unlock(&g_error_mutex);
        return -1;
    }

    if (g_error_ctx.runtimes[idx].state == ERROR_STATE_ACTIVE) {
        update_error_state(idx, ERROR_STATE_ACKNOWLEDGED);
        pthread_mutex_unlock(&g_error_mutex);
        return 0;
    }

    pthread_mutex_unlock(&g_error_mutex);
    return -2;
}

int error_handler_acknowledge_all(void) {
    pthread_mutex_lock(&g_error_mutex);

    int count = 0;
    for (uint32_t i = 0; i < g_error_ctx.registered_count; i++) {
        if (g_error_ctx.runtimes[i].state == ERROR_STATE_ACTIVE) {
            update_error_state(i, ERROR_STATE_ACKNOWLEDGED);
            count++;
        }
    }

    pthread_mutex_unlock(&g_error_mutex);
    return count;
}

int error_handler_reset(uint32_t code) {
    pthread_mutex_lock(&g_error_mutex);

    int idx = find_error_index(code);
    if (idx < 0) {
        pthread_mutex_unlock(&g_error_mutex);
        return -1;
    }

    update_error_state(idx, ERROR_STATE_INACTIVE);

    pthread_mutex_unlock(&g_error_mutex);
    return 0;
}

int error_handler_get_state(uint32_t code, error_state_t *state) {
    if (state == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_error_mutex);

    int idx = find_error_index(code);
    if (idx < 0) {
        pthread_mutex_unlock(&g_error_mutex);
        return -1;
    }

    *state = g_error_ctx.runtimes[idx].state;

    pthread_mutex_unlock(&g_error_mutex);
    return 0;
}

int error_handler_get_active_errors(uint32_t *codes, uint32_t max_count, uint32_t *actual_count) {
    if (codes == NULL || actual_count == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_error_mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < g_error_ctx.registered_count && count < max_count; i++) {
        if (g_error_ctx.runtimes[i].state == ERROR_STATE_ACTIVE ||
            g_error_ctx.runtimes[i].state == ERROR_STATE_ACKNOWLEDGED) {
            codes[count++] = g_error_ctx.runtimes[i].code;
        }
    }

    *actual_count = count;

    pthread_mutex_unlock(&g_error_mutex);
    return 0;
}

uint32_t error_handler_get_active_count(void) {
    pthread_mutex_lock(&g_error_mutex);
    uint32_t count = g_error_ctx.active_count;
    pthread_mutex_unlock(&g_error_mutex);
    return count;
}

uint32_t error_handler_get_latest_error(void) {
    pthread_mutex_lock(&g_error_mutex);
    uint32_t code = g_error_ctx.latest_error_code;
    pthread_mutex_unlock(&g_error_mutex);
    return code;
}

int error_handler_get_runtime(uint32_t code, error_runtime_t *runtime) {
    if (runtime == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_error_mutex);

    int idx = find_error_index(code);
    if (idx < 0) {
        pthread_mutex_unlock(&g_error_mutex);
        return -1;
    }

    memcpy(runtime, &g_error_ctx.runtimes[idx], sizeof(error_runtime_t));

    pthread_mutex_unlock(&g_error_mutex);
    return 0;
}

int error_handler_register_callback(error_state_callback_t callback) {
    pthread_mutex_lock(&g_error_mutex);
    g_state_callback = callback;
    pthread_mutex_unlock(&g_error_mutex);
    return 0;
}

int error_handler_to_json(cJSON *json_root) {
    if (json_root == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_error_mutex);

    cJSON_AddNumberToObject(json_root, "registered_count", g_error_ctx.registered_count);
    cJSON_AddNumberToObject(json_root, "active_count", g_error_ctx.active_count);
    cJSON_AddNumberToObject(json_root, "latest_error_code", g_error_ctx.latest_error_code);

    cJSON *errors_array = cJSON_CreateArray();
    for (uint32_t i = 0; i < g_error_ctx.registered_count; i++) {
        cJSON *error_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(error_obj, "code", g_error_ctx.definitions[i].code);
        cJSON_AddStringToObject(error_obj, "name", g_error_ctx.definitions[i].name);
        cJSON_AddStringToObject(error_obj, "message", g_error_ctx.definitions[i].message);
        cJSON_AddStringToObject(error_obj, "severity",
                                error_severity_to_string(g_error_ctx.definitions[i].severity));
        cJSON_AddStringToObject(error_obj, "state",
                                error_state_to_string(g_error_ctx.runtimes[i].state));
        cJSON_AddBoolToObject(error_obj, "enabled", g_error_ctx.definitions[i].enabled);
        cJSON_AddNumberToObject(error_obj, "threshold", g_error_ctx.definitions[i].threshold);
        cJSON_AddNumberToObject(error_obj, "last_value", g_error_ctx.runtimes[i].last_value);
        cJSON_AddNumberToObject(error_obj, "occurrence_count", g_error_ctx.runtimes[i].occurrence_count);
        if (g_error_ctx.runtimes[i].trigger_timestamp > 0) {
            cJSON_AddNumberToObject(error_obj, "trigger_timestamp",
                                    g_error_ctx.runtimes[i].trigger_timestamp);
        }
        cJSON_AddItemToArray(errors_array, error_obj);
    }
    cJSON_AddItemToObject(json_root, "errors", errors_array);

    pthread_mutex_unlock(&g_error_mutex);
    return 0;
}

int error_handler_active_to_json(cJSON *json_root) {
    if (json_root == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_error_mutex);

    cJSON_AddNumberToObject(json_root, "active_count", g_error_ctx.active_count);

    cJSON *errors_array = cJSON_CreateArray();
    for (uint32_t i = 0; i < g_error_ctx.registered_count; i++) {
        if (g_error_ctx.runtimes[i].state == ERROR_STATE_ACTIVE ||
            g_error_ctx.runtimes[i].state == ERROR_STATE_ACKNOWLEDGED) {
            cJSON *error_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(error_obj, "code", g_error_ctx.definitions[i].code);
            cJSON_AddStringToObject(error_obj, "name", g_error_ctx.definitions[i].name);
            cJSON_AddStringToObject(error_obj, "message", g_error_ctx.definitions[i].message);
            cJSON_AddStringToObject(error_obj, "severity",
                                    error_severity_to_string(g_error_ctx.definitions[i].severity));
            cJSON_AddStringToObject(error_obj, "state",
                                    error_state_to_string(g_error_ctx.runtimes[i].state));
            cJSON_AddNumberToObject(error_obj, "last_value", g_error_ctx.runtimes[i].last_value);
            cJSON_AddNumberToObject(error_obj, "trigger_timestamp",
                                    g_error_ctx.runtimes[i].trigger_timestamp);
            cJSON_AddItemToArray(errors_array, error_obj);
        }
    }
    cJSON_AddItemToObject(json_root, "active_errors", errors_array);

    pthread_mutex_unlock(&g_error_mutex);
    return 0;
}

int error_handler_error_to_json(uint32_t code, cJSON *json_root) {
    if (json_root == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_error_mutex);

    int idx = find_error_index(code);
    if (idx < 0) {
        pthread_mutex_unlock(&g_error_mutex);
        return -1;
    }

    cJSON_AddNumberToObject(json_root, "code", g_error_ctx.definitions[idx].code);
    cJSON_AddStringToObject(json_root, "name", g_error_ctx.definitions[idx].name);
    cJSON_AddStringToObject(json_root, "message", g_error_ctx.definitions[idx].message);
    cJSON_AddStringToObject(json_root, "severity",
                            error_severity_to_string(g_error_ctx.definitions[idx].severity));
    cJSON_AddStringToObject(json_root, "state",
                            error_state_to_string(g_error_ctx.runtimes[idx].state));
    cJSON_AddBoolToObject(json_root, "enabled", g_error_ctx.definitions[idx].enabled);
    cJSON_AddNumberToObject(json_root, "threshold", g_error_ctx.definitions[idx].threshold);
    cJSON_AddNumberToObject(json_root, "hysteresis", g_error_ctx.definitions[idx].hysteresis);
    cJSON_AddNumberToObject(json_root, "delay_ms", g_error_ctx.definitions[idx].delay_ms);
    cJSON_AddNumberToObject(json_root, "last_value", g_error_ctx.runtimes[idx].last_value);
    cJSON_AddNumberToObject(json_root, "occurrence_count", g_error_ctx.runtimes[idx].occurrence_count);
    if (g_error_ctx.runtimes[idx].trigger_timestamp > 0) {
        cJSON_AddNumberToObject(json_root, "trigger_timestamp",
                                g_error_ctx.runtimes[idx].trigger_timestamp);
    }

    pthread_mutex_unlock(&g_error_mutex);
    return 0;
}

void error_handler_enable_modbus(bool enable) {
    pthread_mutex_lock(&g_error_mutex);
    g_error_ctx.modbus_enabled = enable;
    pthread_mutex_unlock(&g_error_mutex);
}

int error_handler_sync_to_modbus(void) {
    /* TODO: 實作 Modbus 暫存器同步 */
    /* 此處需要整合 control_logic_common 的 Modbus 寫入 API */

    pthread_mutex_lock(&g_error_mutex);

    for (uint32_t i = 0; i < g_error_ctx.registered_count; i++) {
        if (!g_error_ctx.runtimes[i].modbus_synced) {
            /* 計算暫存器位址 */
            /* uint32_t reg_addr = ERROR_MODBUS_STATUS_BASE + i; */

            /* 計算狀態值 (0=正常, 1=觸發) */
            /* uint16_t value = (g_error_ctx.runtimes[i].state >= ERROR_STATE_ACTIVE) ? 1 : 0; */

            /* control_logic_write_register(reg_addr, value, 100); */

            g_error_ctx.runtimes[i].modbus_synced = true;
        }
    }

    /* 同步活動錯誤數量 */
    /* control_logic_write_register(ERROR_MODBUS_ACTIVE_COUNT, g_error_ctx.active_count, 100); */

    /* 同步最新錯誤碼 */
    /* control_logic_write_register(ERROR_MODBUS_LATEST_CODE, g_error_ctx.latest_error_code, 100); */

    pthread_mutex_unlock(&g_error_mutex);
    return 0;
}

int error_handler_process_modbus_commands(void) {
    /* TODO: 實作 Modbus 命令處理 */
    /* 讀取 ERROR_MODBUS_ACK_CMD 和 ERROR_MODBUS_RESET_CMD 暫存器 */
    /* 如果有寫入值，執行對應的 acknowledge 或 reset 操作 */

    return 0;
}

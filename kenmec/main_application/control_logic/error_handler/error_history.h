/**
 * @file error_history.h
 * @brief 錯誤碼歷史記錄模組定義
 *
 * 本模組提供錯誤碼歷史記錄的持久化儲存功能：
 * - 錯誤發生時自動記錄到檔案
 * - 支援查詢最近 N 筆記錄
 * - 支援統計查詢 (發生次數、最後發生時間等)
 * - 自動清理過舊的記錄
 */

#ifndef ERROR_HISTORY_H
#define ERROR_HISTORY_H

#include <stdint.h>
#include <stdbool.h>
#include "error_handler.h"
#include "cJSON.h"

/* ========== 常數定義 ========== */

/** 預設歷史記錄檔案路徑 */
#define ERROR_HISTORY_DEFAULT_PATH "/var/kenmec/logs/error_history.json"

/** 最大歷史記錄數量 */
#define ERROR_HISTORY_MAX_RECORDS 1000

/** 歷史記錄版本 */
#define ERROR_HISTORY_VERSION "1.0"

/* ========== 結構定義 ========== */

/**
 * @brief 單筆錯誤歷史記錄
 */
typedef struct {
    uint32_t code;              /**< 錯誤碼 ID */
    char name[64];              /**< 錯誤碼名稱 */
    uint32_t timestamp;         /**< 發生時間 (Unix timestamp) */
    float value;                /**< 觸發時的感測值 */
    float threshold;            /**< 觸發閾值 */
    error_severity_t severity;  /**< 嚴重等級 */
    uint32_t duration_sec;      /**< 持續時間 (秒，僅已恢復的記錄有效) */
    bool recovered;             /**< 是否已恢復 */
} error_history_record_t;

/**
 * @brief 錯誤碼統計資訊
 */
typedef struct {
    uint32_t code;              /**< 錯誤碼 ID */
    uint32_t total_count;       /**< 總發生次數 */
    uint32_t last_timestamp;    /**< 最後發生時間 */
    uint32_t total_duration;    /**< 總持續時間 (秒) */
    float max_value;            /**< 最大觸發值 */
} error_history_stats_t;

/* ========== API 函數宣告 ========== */

/**
 * @brief 初始化歷史記錄模組
 *
 * @param log_path 歷史記錄檔案路徑，NULL 使用預設路徑
 * @return 成功返回 0，失敗返回負值
 */
int error_history_init(const char *log_path);

/**
 * @brief 清理歷史記錄模組資源
 */
void error_history_cleanup(void);

/**
 * @brief 記錄錯誤發生
 *
 * @param runtime 錯誤運行時狀態
 * @param def 錯誤定義
 * @return 成功返回 0
 */
int error_history_log(const error_runtime_t *runtime, const error_definition_t *def);

/**
 * @brief 記錄錯誤恢復
 *
 * @param code 錯誤碼 ID
 * @return 成功返回 0
 */
int error_history_log_recovery(uint32_t code);

/**
 * @brief 獲取最近 N 筆歷史記錄
 *
 * @param count 要獲取的記錄數量
 * @param json_out 輸出的 JSON 物件
 * @return 成功返回 0
 */
int error_history_get_recent(uint32_t count, cJSON *json_out);

/**
 * @brief 獲取特定錯誤碼的歷史記錄
 *
 * @param code 錯誤碼 ID
 * @param count 要獲取的記錄數量
 * @param json_out 輸出的 JSON 物件
 * @return 成功返回 0
 */
int error_history_get_by_code(uint32_t code, uint32_t count, cJSON *json_out);

/**
 * @brief 獲取錯誤碼統計資訊
 *
 * @param code 錯誤碼 ID，0 表示獲取所有錯誤碼的統計
 * @param json_out 輸出的 JSON 物件
 * @return 成功返回 0
 */
int error_history_get_statistics(uint32_t code, cJSON *json_out);

/**
 * @brief 清除所有歷史記錄
 *
 * @return 成功返回 0
 */
int error_history_clear(void);

/**
 * @brief 清除指定時間之前的歷史記錄
 *
 * @param before_timestamp Unix 時間戳
 * @return 成功返回清除的記錄數量
 */
int error_history_clear_before(uint32_t before_timestamp);

/**
 * @brief 強制將快取寫入檔案
 *
 * @return 成功返回 0
 */
int error_history_flush(void);

/**
 * @brief 獲取歷史記錄數量
 *
 * @return 記錄數量
 */
uint32_t error_history_get_count(void);

#endif /* ERROR_HISTORY_H */

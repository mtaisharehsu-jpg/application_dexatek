/**
 * @file error_handler.h
 * @brief 通用錯誤處理框架定義
 *
 * 本模組提供統一的錯誤碼管理機制，包含：
 * - 錯誤碼定義與註冊
 * - 錯誤狀態追蹤
 * - 延遲觸發機制
 * - Modbus 暫存器整合
 * - JSON API 支援
 *
 * 錯誤碼範圍分配：
 * - 1000-1999: 系統通用錯誤
 * - 2000-2999: LS80 機型錯誤
 * - 3000-3999: LS300D 機型錯誤
 * - 4000-4999: LX1400 機型錯誤
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "cJSON.h"

/* ========== 常數定義 ========== */

/** 最大可註冊錯誤碼數量 */
#define ERROR_HANDLER_MAX_ERRORS 256

/** 錯誤碼名稱最大長度 */
#define ERROR_NAME_MAX_LEN 64

/** 錯誤訊息最大長度 */
#define ERROR_MESSAGE_MAX_LEN 256

/** Modbus 暫存器基址 */
#define ERROR_MODBUS_STATUS_BASE 43000    // 錯誤狀態起始位址
#define ERROR_MODBUS_ACTIVE_COUNT 43200   // 活動錯誤數量
#define ERROR_MODBUS_LATEST_CODE 43201    // 最新錯誤碼
#define ERROR_MODBUS_ACK_CMD 43300        // 確認命令
#define ERROR_MODBUS_RESET_CMD 43301      // 重置命令

/* ========== 枚舉定義 ========== */

/**
 * @brief 錯誤碼嚴重等級
 */
typedef enum {
    ERROR_SEVERITY_MINOR = 0,     /**< 輕微 - 黃色警告 */
    ERROR_SEVERITY_MAJOR = 1,     /**< 中度 - 橙色警告 */
    ERROR_SEVERITY_CRITICAL = 2   /**< 嚴重 - 紅色警報，可能停機 */
} error_severity_t;

/**
 * @brief 錯誤碼復歸模式
 */
typedef enum {
    ERROR_RECOVERY_AUTO = 0,      /**< 自動復歸 - 條件消失後自動清除 */
    ERROR_RECOVERY_MANUAL = 1     /**< 手動復歸 - 需人工確認後清除 */
} error_recovery_mode_t;

/**
 * @brief 錯誤碼狀態
 */
typedef enum {
    ERROR_STATE_INACTIVE = 0,     /**< 未觸發 */
    ERROR_STATE_PENDING = 1,      /**< 等待延遲確認中 */
    ERROR_STATE_ACTIVE = 2,       /**< 已觸發 */
    ERROR_STATE_ACKNOWLEDGED = 3  /**< 已確認，等待條件消失後復歸 */
} error_state_t;

/**
 * @brief 錯誤碼條件類型
 */
typedef enum {
    ERROR_COND_THRESHOLD_HIGH = 0,    /**< 高於閾值觸發 */
    ERROR_COND_THRESHOLD_LOW = 1,     /**< 低於閾值觸發 */
    ERROR_COND_EQUAL = 2,             /**< 等於特定值觸發 */
    ERROR_COND_NOT_EQUAL = 3,         /**< 不等於特定值觸發 */
    ERROR_COND_CUSTOM = 4             /**< 自定義條件函數 */
} error_condition_type_t;

/* ========== 結構定義 ========== */

/**
 * @brief 錯誤碼定義結構
 *
 * 用於靜態定義錯誤碼的所有屬性
 */
typedef struct {
    uint32_t code;                          /**< 錯誤碼 ID */
    char name[ERROR_NAME_MAX_LEN];          /**< 錯誤碼名稱 */
    char message[ERROR_MESSAGE_MAX_LEN];    /**< 告警訊息 */
    error_severity_t severity;              /**< 嚴重等級 */
    error_recovery_mode_t recovery;         /**< 復歸模式 */
    error_condition_type_t cond_type;       /**< 條件類型 */
    float threshold;                        /**< 觸發閾值 */
    float hysteresis;                       /**< 重置遲滯值 */
    uint32_t delay_ms;                      /**< 延遲時間 (毫秒) */
    bool enabled;                           /**< 是否啟用 */

    /** 獲取當前感測值的函數指標 */
    float (*get_value)(void);

    /** 自定義條件檢查函數指標 (cond_type=CUSTOM 時使用) */
    int (*condition_check)(void);

    /** 錯誤觸發時的動作處理函數指標 */
    void (*action_handler)(uint32_t code, error_state_t new_state);
} error_definition_t;

/**
 * @brief 錯誤碼運行時狀態結構
 *
 * 用於追蹤每個錯誤碼的即時狀態
 */
typedef struct {
    uint32_t code;                  /**< 錯誤碼 ID */
    error_state_t state;            /**< 當前狀態 */
    uint32_t pending_start_ms;      /**< 進入 PENDING 狀態的時間戳 */
    uint32_t trigger_timestamp;     /**< 觸發時間戳 (Unix time) */
    uint32_t occurrence_count;      /**< 累計發生次數 */
    float last_value;               /**< 最後讀取的感測值 */
    bool modbus_synced;             /**< Modbus 同步標記 */
} error_runtime_t;

/**
 * @brief 錯誤處理器上下文結構
 */
typedef struct {
    error_definition_t definitions[ERROR_HANDLER_MAX_ERRORS];   /**< 錯誤定義陣列 */
    error_runtime_t runtimes[ERROR_HANDLER_MAX_ERRORS];         /**< 運行時狀態陣列 */
    uint32_t registered_count;      /**< 已註冊錯誤碼數量 */
    uint32_t active_count;          /**< 當前活動錯誤數量 */
    uint32_t latest_error_code;     /**< 最新觸發的錯誤碼 */
    bool initialized;               /**< 初始化標記 */
    bool modbus_enabled;            /**< Modbus 整合是否啟用 */
    bool history_enabled;           /**< 歷史記錄是否啟用 */
} error_handler_context_t;

/* ========== 回調函數類型 ========== */

/**
 * @brief 狀態變更回調函數類型
 *
 * @param code 錯誤碼 ID
 * @param old_state 舊狀態
 * @param new_state 新狀態
 */
typedef void (*error_state_callback_t)(uint32_t code, error_state_t old_state, error_state_t new_state);

/* ========== API 函數宣告 ========== */

/* ----- 初始化與生命週期 ----- */

/**
 * @brief 初始化錯誤處理模組
 *
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int error_handler_init(void);

/**
 * @brief 清理錯誤處理模組資源
 */
void error_handler_cleanup(void);

/**
 * @brief 重置所有錯誤狀態
 *
 * @return 成功返回 0
 */
int error_handler_reset_all(void);

/* ----- 錯誤碼註冊 ----- */

/**
 * @brief 註冊錯誤碼定義
 *
 * @param def 錯誤碼定義結構指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int error_handler_register(const error_definition_t *def);

/**
 * @brief 批量註冊錯誤碼定義
 *
 * @param defs 錯誤碼定義陣列
 * @param count 定義數量
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int error_handler_register_batch(const error_definition_t *defs, uint32_t count);

/**
 * @brief 從 JSON 配置檔載入錯誤碼定義
 *
 * @param config_path JSON 配置檔路徑
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int error_handler_load_config(const char *config_path);

/* ----- 錯誤處理週期 ----- */

/**
 * @brief 處理所有錯誤碼檢查 (週期性呼叫)
 *
 * 此函數應在主控制迴圈中週期性呼叫，建議間隔 100-1000ms
 *
 * @return 成功返回 0
 */
int error_handler_process(void);

/**
 * @brief 手動設定錯誤狀態
 *
 * 用於外部直接觸發錯誤 (如通訊失敗)
 *
 * @param code 錯誤碼 ID
 * @param active true 表示觸發，false 表示清除
 * @return 成功返回 0，錯誤碼不存在返回 -1
 */
int error_handler_set_error(uint32_t code, bool active);

/* ----- 錯誤確認與重置 ----- */

/**
 * @brief 確認錯誤 (Acknowledge)
 *
 * 將錯誤狀態從 ACTIVE 變更為 ACKNOWLEDGED
 *
 * @param code 錯誤碼 ID
 * @return 成功返回 0，錯誤碼不存在或狀態不正確返回負值
 */
int error_handler_acknowledge(uint32_t code);

/**
 * @brief 確認所有活動錯誤
 *
 * @return 確認的錯誤數量
 */
int error_handler_acknowledge_all(void);

/**
 * @brief 手動重置錯誤
 *
 * @param code 錯誤碼 ID
 * @return 成功返回 0，錯誤碼不存在返回負值
 */
int error_handler_reset(uint32_t code);

/* ----- 狀態查詢 ----- */

/**
 * @brief 獲取錯誤碼當前狀態
 *
 * @param code 錯誤碼 ID
 * @param state 輸出參數，返回當前狀態
 * @return 成功返回 0，錯誤碼不存在返回 -1
 */
int error_handler_get_state(uint32_t code, error_state_t *state);

/**
 * @brief 獲取所有活動錯誤碼列表
 *
 * @param codes 輸出陣列，存放活動錯誤碼
 * @param max_count 陣列最大容量
 * @param actual_count 輸出參數，返回實際活動錯誤數量
 * @return 成功返回 0
 */
int error_handler_get_active_errors(uint32_t *codes, uint32_t max_count, uint32_t *actual_count);

/**
 * @brief 獲取活動錯誤數量
 *
 * @return 活動錯誤數量
 */
uint32_t error_handler_get_active_count(void);

/**
 * @brief 獲取最新觸發的錯誤碼
 *
 * @return 最新錯誤碼 ID，無錯誤時返回 0
 */
uint32_t error_handler_get_latest_error(void);

/**
 * @brief 獲取錯誤碼運行時資訊
 *
 * @param code 錯誤碼 ID
 * @param runtime 輸出參數，返回運行時狀態副本
 * @return 成功返回 0，錯誤碼不存在返回 -1
 */
int error_handler_get_runtime(uint32_t code, error_runtime_t *runtime);

/* ----- 回調註冊 ----- */

/**
 * @brief 註冊狀態變更回調函數
 *
 * @param callback 回調函數指標
 * @return 成功返回 0
 */
int error_handler_register_callback(error_state_callback_t callback);

/* ----- JSON API ----- */

/**
 * @brief 將所有錯誤狀態輸出為 JSON
 *
 * @param json_root cJSON 根節點
 * @return 成功返回 0
 */
int error_handler_to_json(cJSON *json_root);

/**
 * @brief 將活動錯誤輸出為 JSON
 *
 * @param json_root cJSON 根節點
 * @return 成功返回 0
 */
int error_handler_active_to_json(cJSON *json_root);

/**
 * @brief 將特定錯誤碼資訊輸出為 JSON
 *
 * @param code 錯誤碼 ID
 * @param json_root cJSON 根節點
 * @return 成功返回 0，錯誤碼不存在返回 -1
 */
int error_handler_error_to_json(uint32_t code, cJSON *json_root);

/* ----- Modbus 整合 ----- */

/**
 * @brief 啟用/停用 Modbus 整合
 *
 * @param enable true 啟用，false 停用
 */
void error_handler_enable_modbus(bool enable);

/**
 * @brief 同步錯誤狀態到 Modbus 暫存器
 *
 * @return 成功返回 0
 */
int error_handler_sync_to_modbus(void);

/**
 * @brief 處理 Modbus 命令 (確認/重置)
 *
 * @return 成功返回 0
 */
int error_handler_process_modbus_commands(void);

/* ----- 輔助函數 ----- */

/**
 * @brief 取得嚴重等級字串
 *
 * @param severity 嚴重等級
 * @return 嚴重等級字串
 */
const char *error_severity_to_string(error_severity_t severity);

/**
 * @brief 取得狀態字串
 *
 * @param state 錯誤狀態
 * @return 狀態字串
 */
const char *error_state_to_string(error_state_t state);

/**
 * @brief 取得當前時間戳 (毫秒)
 *
 * @return 時間戳
 */
uint32_t error_handler_get_timestamp_ms(void);

#endif /* ERROR_HANDLER_H */

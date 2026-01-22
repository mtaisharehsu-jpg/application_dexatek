/**
 * @file error_notify.h
 * @brief 錯誤通知接口定義 (預留架構)
 *
 * 本模組定義錯誤通知的抽象接口，支援未來擴展：
 * - SNMP trap
 * - MQTT 訊息
 * - Webhook
 * - Email
 *
 * 目前僅定義接口，實際通知功能待後續實作。
 */

#ifndef ERROR_NOTIFY_H
#define ERROR_NOTIFY_H

#include <stdint.h>
#include <stdbool.h>
#include "error_handler.h"

/* ========== 常數定義 ========== */

/** 最大通知處理器數量 */
#define ERROR_NOTIFY_MAX_HANDLERS 4

/** 通知主機最大長度 */
#define ERROR_NOTIFY_HOST_MAX_LEN 128

/** 通知主題/URL 最大長度 */
#define ERROR_NOTIFY_TOPIC_MAX_LEN 256

/* ========== 枚舉定義 ========== */

/**
 * @brief 通知類型
 */
typedef enum {
    ERROR_NOTIFY_TYPE_NONE = 0,      /**< 未設定 */
    ERROR_NOTIFY_TYPE_LOG = 1,       /**< 本地日誌 (預設啟用) */
    ERROR_NOTIFY_TYPE_SNMP = 2,      /**< SNMP trap (預留) */
    ERROR_NOTIFY_TYPE_MQTT = 3,      /**< MQTT 訊息 (預留) */
    ERROR_NOTIFY_TYPE_WEBHOOK = 4,   /**< Webhook 回調 (預留) */
    ERROR_NOTIFY_TYPE_EMAIL = 5      /**< Email 通知 (預留) */
} error_notify_type_t;

/**
 * @brief 通知嚴重等級過濾
 */
typedef enum {
    ERROR_NOTIFY_FILTER_ALL = 0,        /**< 所有等級都通知 */
    ERROR_NOTIFY_FILTER_MAJOR_UP = 1,   /**< 只通知 major 及以上 */
    ERROR_NOTIFY_FILTER_CRITICAL = 2    /**< 只通知 critical */
} error_notify_filter_t;

/* ========== 結構定義 ========== */

/**
 * @brief SNMP 通知配置 (預留)
 */
typedef struct {
    char community[64];             /**< SNMP community string */
    char trap_host[ERROR_NOTIFY_HOST_MAX_LEN];  /**< Trap 目標主機 */
    uint16_t trap_port;             /**< Trap 目標埠號 (預設 162) */
    char enterprise_oid[128];       /**< Enterprise OID */
} error_notify_snmp_config_t;

/**
 * @brief MQTT 通知配置 (預留)
 */
typedef struct {
    char broker[ERROR_NOTIFY_HOST_MAX_LEN];     /**< MQTT Broker 位址 */
    uint16_t port;                  /**< MQTT 埠號 (預設 1883) */
    char topic[ERROR_NOTIFY_TOPIC_MAX_LEN];     /**< 發布主題 */
    char client_id[64];             /**< Client ID */
    char username[64];              /**< 使用者名稱 */
    char password[64];              /**< 密碼 */
    uint8_t qos;                    /**< QoS 等級 (0, 1, 2) */
    bool use_tls;                   /**< 是否使用 TLS */
} error_notify_mqtt_config_t;

/**
 * @brief Webhook 通知配置 (預留)
 */
typedef struct {
    char url[ERROR_NOTIFY_TOPIC_MAX_LEN];       /**< Webhook URL */
    char auth_token[128];           /**< 認證 Token */
    uint32_t timeout_ms;            /**< 請求超時時間 */
    bool verify_ssl;                /**< 是否驗證 SSL 憑證 */
} error_notify_webhook_config_t;

/**
 * @brief Email 通知配置 (預留)
 */
typedef struct {
    char smtp_server[ERROR_NOTIFY_HOST_MAX_LEN];  /**< SMTP 伺服器 */
    uint16_t smtp_port;             /**< SMTP 埠號 */
    char from_address[128];         /**< 寄件人地址 */
    char to_addresses[256];         /**< 收件人地址 (逗號分隔) */
    char username[64];              /**< SMTP 使用者名稱 */
    char password[64];              /**< SMTP 密碼 */
    bool use_tls;                   /**< 是否使用 TLS */
} error_notify_email_config_t;

/**
 * @brief 通知配置結構 (統一介面)
 */
typedef struct {
    error_notify_type_t type;       /**< 通知類型 */
    bool enabled;                   /**< 是否啟用 */
    error_notify_filter_t filter;   /**< 嚴重等級過濾 */

    union {
        error_notify_snmp_config_t snmp;
        error_notify_mqtt_config_t mqtt;
        error_notify_webhook_config_t webhook;
        error_notify_email_config_t email;
    } config;
} error_notify_config_t;

/**
 * @brief 通知處理器介面
 */
typedef struct {
    error_notify_type_t type;       /**< 處理器類型 */

    /**
     * @brief 初始化處理器
     * @param config 通知配置
     * @return 成功返回 0
     */
    int (*init)(const error_notify_config_t *config);

    /**
     * @brief 發送通知
     * @param code 錯誤碼
     * @param severity 嚴重等級
     * @param name 錯誤名稱
     * @param message 錯誤訊息
     * @return 成功返回 0
     */
    int (*send)(uint32_t code, error_severity_t severity,
                const char *name, const char *message);

    /**
     * @brief 清理處理器資源
     */
    void (*cleanup)(void);
} error_notify_handler_t;

/* ========== API 函數宣告 ========== */

/**
 * @brief 初始化通知模組
 *
 * @return 成功返回 0
 */
int error_notify_init(void);

/**
 * @brief 清理通知模組資源
 */
void error_notify_cleanup(void);

/**
 * @brief 註冊通知處理器
 *
 * @param handler 處理器結構指標
 * @return 成功返回 0，已滿返回 -1
 */
int error_notify_register_handler(const error_notify_handler_t *handler);

/**
 * @brief 配置通知
 *
 * @param config 通知配置
 * @return 成功返回 0
 */
int error_notify_configure(const error_notify_config_t *config);

/**
 * @brief 發送錯誤通知
 *
 * 此函數會呼叫所有已註冊且啟用的通知處理器
 *
 * @param code 錯誤碼
 * @param severity 嚴重等級
 * @param name 錯誤名稱
 * @param message 錯誤訊息
 * @return 成功發送的處理器數量
 */
int error_notify_send(uint32_t code, error_severity_t severity,
                      const char *name, const char *message);

/**
 * @brief 從 JSON 配置檔載入通知設定
 *
 * @param config_path 配置檔路徑
 * @return 成功返回 0
 */
int error_notify_load_config(const char *config_path);

/**
 * @brief 獲取通知類型字串
 *
 * @param type 通知類型
 * @return 類型字串
 */
const char *error_notify_type_to_string(error_notify_type_t type);

/* ========== 內建處理器 (Log) ========== */

/**
 * @brief 取得本地日誌處理器
 *
 * @return 處理器結構指標
 */
const error_notify_handler_t *error_notify_get_log_handler(void);

#endif /* ERROR_NOTIFY_H */

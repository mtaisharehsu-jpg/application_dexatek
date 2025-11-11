/**
 * @file control_logic_manager.h
 * @brief 控制邏輯管理器頭文件
 *
 * 本文件定義控制邏輯管理器的主要功能，負責管理和協調所有控制邏輯的執行。
 * 主要功能包括：
 * - 控制邏輯的初始化和啟動/停止
 * - 多執行緒管理和調度
 * - 控制邏輯處理循環
 * - 不同機型的控制邏輯適配
 * - 系統狀態監控
 */

#ifndef CONTROL_LOGIC_MANAGER_H
#define CONTROL_LOGIC_MANAGER_H

#include "kenmec/main_application/redfish/include/cJSON.h"
#include "kenmec/main_application/control_logic/control_logic_update.h"
#include "kenmec/main_application/control_logic/control_hardware.h"
#include "kenmec/main_application/control_logic/control_logic_register.h"
#include "kenmec/main_application/control_logic/control_logic_common.h"
#include "kenmec/main_application/control_logic/control_logic_config.h"

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
/* 控制邏輯處理週期，單位：毫秒 */
#define CONTROL_LOGIC_PROCESS_INTERVAL_MS 1000  // 每1000毫秒（1秒）執行一次控制邏輯

/*---------------------------------------------------------------------------
                            Type Definitions
 ---------------------------------------------------------------------------*/

/**
 * @brief 控制邏輯結構型別定義
 */
typedef struct control_logic_t ControlLogic;

/**
 * @brief 控制邏輯結構
 *
 * 定義單一控制邏輯的完整資訊，包括執行函數、執行緒控制和時間戳記
 */
struct control_logic_t {
    int (*func)(ControlLogic* ptr);    /* 控制邏輯執行函數指標 */
    pthread_t thread_handle;           /* POSIX 執行緒控制代碼 */
    uint32_t latest_timestamp_ms;      /* 最近一次執行的時間戳記（毫秒） */
    int (*init)(void);                 /* 控制邏輯初始化函數指標 */
};

/*---------------------------------------------------------------------------
                            Function Prototypes
 ---------------------------------------------------------------------------*/

/**
 * @brief 初始化控制邏輯元件
 *
 * 初始化控制邏輯管理器，設定所有必要的資源和狀態。
 * 此函數應在系統啟動時呼叫一次。
 *
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_manager_init(void);

/**
 * @brief 啟動控制邏輯處理
 *
 * 啟動控制邏輯的執行緒，開始週期性執行控制邏輯。
 * 呼叫此函數後，控制邏輯將按照 CONTROL_LOGIC_PROCESS_INTERVAL_MS 定義的週期執行。
 *
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_manager_start(void);

/**
 * @brief 停止控制邏輯處理
 *
 * 停止所有控制邏輯的執行，終止執行緒。
 * 呼叫此函數後，控制邏輯將停止執行，但不會釋放資源。
 *
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_manager_stop(void);

/**
 * @brief 清理並反初始化控制邏輯元件
 *
 * 釋放所有控制邏輯管理器使用的資源。
 * 此函數應在系統關閉時呼叫。
 */
void control_logic_manager_cleanup(void);

/**
 * @brief 處理控制邏輯操作
 *
 * 執行一次控制邏輯處理循環，包括讀取感測器數據、執行控制演算法、
 * 輸出控制命令等操作。此函數由內部執行緒週期性呼叫。
 */
void control_logic_manager_process(void);

/**
 * @brief 檢查控制邏輯是否正在運行
 *
 * 查詢控制邏輯管理器的運行狀態。
 *
 * @return 如果正在運行返回 true，否則返回 false
 */
bool control_logic_manager_is_running(void);

/**
 * @brief 獲取控制邏輯陣列的大小
 *
 * 返回系統中配置的控制邏輯總數。
 *
 * @return 控制邏輯陣列的大小（控制邏輯數量）
 */
int control_logic_manager_number_of_control_logics(void);

/**
 * @brief 設定控制邏輯函數指標
 *
 * 根據機型類型設定對應的控制邏輯函數指標。
 * 不同的機型（如 LS80、LX1400）使用不同的控制邏輯實現。
 *
 * @param machine_type 機型類型（參考 control_logic_machine_type_t）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_manager_set_function_pointer(int machine_type);

/**
 * @brief 重新初始化控制邏輯管理器
 *
 * 重新載入配置並重新初始化控制邏輯管理器。
 * 用於在運行時切換機型或更新配置。
 *
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_manager_reinit(void);

#endif /* CONTROL_LOGIC_MANAGER_H */ 
/**
 * @file control_logic_common.h
 * @brief 控制邏輯通用功能頭文件
 *
 * 本文件定義控制邏輯系統的通用功能函數，提供暫存器讀寫和 API 介面。
 * 主要功能包括：
 * - Modbus 暫存器的讀取和寫入
 * - JSON 格式的資料交換
 * - API 資料格式轉換
 */

#ifndef CONTROL_LOGIC_COMMON_H
#define CONTROL_LOGIC_COMMON_H

/**
 * @brief 讀取 Holding 暫存器
 *
 * 從 Modbus Holding Register 讀取單一暫存器的值。
 * Holding Register 通常用於存儲可讀寫的參數和設定值。
 *
 * @param address 暫存器位址
 * @param value 指向存儲讀取值的指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_read_holding_register(uint32_t address, uint16_t *value);

/**
 * @brief 寫入暫存器
 *
 * 向 Modbus 暫存器寫入單一值。
 * 支援超時設定，避免通訊阻塞。
 *
 * @param address 暫存器位址
 * @param value 要寫入的值
 * @param timeout_ms 超時時間（毫秒），0 表示使用預設超時
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_write_register(uint32_t address, uint16_t value, uint16_t timeout_ms);

/**
 * @brief 將控制邏輯資料附加到 JSON 物件
 *
 * 讀取指定控制邏輯的所有相關資料，並將其格式化為 JSON 格式附加到指定的 JSON 根物件中。
 * 用於 API 回應和資料匯出。
 *
 * @param logic_id 控制邏輯 ID（1-7，對應不同的控制功能）
 * @param json_root 指向 cJSON 根物件的指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_api_data_append_to_json(uint8_t logic_id, cJSON *json_root);

/**
 * @brief 透過 JSON 寫入控制邏輯參數
 *
 * 解析 JSON 格式的資料，並將其寫入到指定控制邏輯的相關暫存器中。
 * 用於 API 請求和批量參數設定。
 *
 * @param logic_id 控制邏輯 ID（1-7，對應不同的控制功能）
 * @param jsonPayload JSON 格式的參數字串
 * @param timeout_ms 每個暫存器寫入的超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_api_write_by_json(uint8_t logic_id, const char *jsonPayload, uint16_t timeout_ms);

#endif /* CONTROL_LOGIC_COMMON_H */ 
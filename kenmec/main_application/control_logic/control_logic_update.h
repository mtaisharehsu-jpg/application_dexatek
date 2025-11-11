/**
 * @file control_logic_update.h
 * @brief 控制邏輯更新介面頭文件
 *
 * 本文件定義控制邏輯與 Modbus 表格之間的資料同步介面。
 * 主要功能包括：
 * - 初始化更新機制
 * - 將資料更新到 Modbus 表格
 * - 從 Modbus 表格載入資料
 */

#ifndef CONTROL_LOGIC_UPDATE_H
#define CONTROL_LOGIC_UPDATE_H

// #include "dexatek/main_application/managers/modbus_manager/modbus_manager.h"

/**
 * @brief 初始化控制邏輯更新模組
 *
 * 初始化控制邏輯與 Modbus 表格之間的資料同步機制。
 * 此函數應在系統啟動時呼叫。
 *
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_update_init(void);

/**
 * @brief 更新資料到 Modbus 表格
 *
 * 將指定的資料寫入到 Modbus 表格的對應位址。
 * 支援多種資料類型（如 uint16_t, float 等）。
 *
 * @param address Modbus 表格位址
 * @param type 資料類型（例如：0=uint16, 1=int16, 2=float）
 * @param value 指向要寫入資料的指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_update_to_modbus_table(uint16_t address, uint8_t type, void *value);

/**
 * @brief 從 Modbus 表格載入資料
 *
 * 從 Modbus 表格的指定位址讀取資料。
 * 支援多種資料類型的讀取。
 *
 * @param address Modbus 表格位址
 * @param type 資料類型（例如：0=uint16, 1=int16, 2=float）
 * @param data 指向存儲讀取資料的緩衝區指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_load_from_modbus_table(uint16_t address, uint8_t type, void *data);

#endif /* CONTROL_LOGIC_UPDATE_H */ 
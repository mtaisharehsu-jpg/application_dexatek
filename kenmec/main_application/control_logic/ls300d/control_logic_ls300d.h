/**
 * @file control_logic_ls300d.h
 * @brief LS300D 機型控制邏輯頭文件
 *
 * 本文件定義 LS300D 機型專用的控制邏輯函數。
 * LS300D 機型包含 7 個控制邏輯模組：
 * 1. 溫度控制 - 監控和調節系統溫度 (雙備援感測器)
 * 2. 壓力控制 - 監控和調節系統壓力 (雙備援感測器)
 * 3. 流量控制 - 監控和調節系統流量
 * 4. 幫浦控制 - 控制主幫浦運行
 * 5. 水泵控制 - 控制冷卻水泵 (中液位停止)
 * 6. 閥門控制 - 控制流量調節閥
 * 7. 雙直流幫浦控制 - 控制備用直流幫浦
 */

#ifndef CONTROL_LOGIC_LS300D_H
#define CONTROL_LOGIC_LS300D_H

#include "kenmec/main_application/control_logic/control_logic_manager.h"

/* ========== LS300D 控制邏輯初始化函數 ========== */

/**
 * @brief 初始化 LS300D 溫度控制邏輯
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_1_temperature_control_init(void);

/**
 * @brief 初始化 LS300D 壓力控制邏輯
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_2_pressure_control_init(void);

/**
 * @brief 初始化 LS300D 流量控制邏輯
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_3_flow_control_init(void);

/**
 * @brief 初始化 LS300D 幫浦控制邏輯
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_4_pump_control_init(void);

/**
 * @brief 初始化 LS300D 水泵控制邏輯
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_5_waterpump_control_init(void);

/**
 * @brief 初始化 LS300D 閥門控制邏輯
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_6_valve_control_init(void);

/**
 * @brief 初始化 LS300D 雙直流幫浦控制邏輯
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_7_2dc_pump_control_init(void);

/* ========== LS300D 控制邏輯執行函數 ========== */

/**
 * @brief 執行 LS300D 溫度控制邏輯
 *
 * 週期性執行溫度監控和控制演算法。
 *
 * @param ptr 指向控制邏輯結構的指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_1_temperature_control(ControlLogic *ptr);

/**
 * @brief 執行 LS300D 壓力控制邏輯
 *
 * 週期性執行壓力監控和控制演算法。
 *
 * @param ptr 指向控制邏輯結構的指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_2_pressure_control(ControlLogic *ptr);

/**
 * @brief 執行 LS300D 流量控制邏輯
 *
 * 週期性執行流量監控和控制演算法。
 *
 * @param ptr 指向控制邏輯結構的指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_3_flow_control(ControlLogic *ptr);

/**
 * @brief 執行 LS300D 幫浦控制邏輯
 *
 * 週期性執行幫浦啟停和速度控制演算法。
 *
 * @param ptr 指向控制邏輯結構的指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_4_pump_control(ControlLogic *ptr);

/**
 * @brief 執行 LS300D 水泵控制邏輯
 *
 * 週期性執行冷卻水泵控制演算法。
 *
 * @param ptr 指向控制邏輯結構的指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_5_waterpump_control(ControlLogic *ptr);

/**
 * @brief 執行 LS300D 閥門控制邏輯
 *
 * 週期性執行閥門開度控制演算法。
 *
 * @param ptr 指向控制邏輯結構的指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_6_valve_control(ControlLogic *ptr);

/**
 * @brief 執行 LS300D 雙直流幫浦控制邏輯
 *
 * 週期性執行備用直流幫浦控制演算法。
 *
 * @param ptr 指向控制邏輯結構的指標
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_7_2dc_pump_control(ControlLogic *ptr);

/* ========== LS300D 配置獲取函數 ========== */

/**
 * @brief 獲取 LS300D 溫度控制配置
 *
 * @param list_size 輸出參數，返回配置列表大小
 * @param list 輸出參數，返回配置列表指標
 * @param file_path 輸出參數，返回配置檔案路徑
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_1_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path);

/**
 * @brief 獲取 LS300D 壓力控制配置
 *
 * @param list_size 輸出參數，返回配置列表大小
 * @param list 輸出參數，返回配置列表指標
 * @param file_path 輸出參數，返回配置檔案路徑
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_2_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path);

/**
 * @brief 獲取 LS300D 流量控制配置
 *
 * @param list_size 輸出參數，返回配置列表大小
 * @param list 輸出參數，返回配置列表指標
 * @param file_path 輸出參數，返回配置檔案路徑
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_3_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path);

/**
 * @brief 獲取 LS300D 幫浦控制配置
 *
 * @param list_size 輸出參數，返回配置列表大小
 * @param list 輸出參數，返回配置列表指標
 * @param file_path 輸出參數，返回配置檔案路徑
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_4_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path);

/**
 * @brief 獲取 LS300D 水泵控制配置
 *
 * @param list_size 輸出參數，返回配置列表大小
 * @param list 輸出參數，返回配置列表指標
 * @param file_path 輸出參數，返回配置檔案路徑
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_5_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path);

/**
 * @brief 獲取 LS300D 閥門控制配置
 *
 * @param list_size 輸出參數，返回配置列表大小
 * @param list 輸出參數，返回配置列表指標
 * @param file_path 輸出參數，返回配置檔案路徑
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_6_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path);

/**
 * @brief 獲取 LS300D 雙直流幫浦控制配置
 *
 * @param list_size 輸出參數，返回配置列表大小
 * @param list 輸出參數，返回配置列表指標
 * @param file_path 輸出參數，返回配置檔案路徑
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_logic_ls300d_7_config_get(uint32_t *list_size, control_logic_register_t **list, char **file_path);

#endif /* CONTROL_LOGIC_LS300D_H */ 
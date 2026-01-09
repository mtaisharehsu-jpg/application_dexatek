# LS300D 硬體抽象層與 API 更新實作計劃

## 概述

本計劃旨在完成 LS300D 機種的硬體抽象層 (HAL) 和 API 整合,使其能夠:
- 正確初始化硬體 I/O 模組
- 透過 Redfish API 讀取和寫入控制邏輯參數
- 與現有 LS80 和 LX1400 機種共存而不產生衝突

## 背景資訊

### 已完成的工作
- ✅ LS300D 機種列舉已定義 (`CONTROL_LOGIC_MACHINE_TYPE_LS300D`)
- ✅ 機種識別邏輯已實作 (字串 "LS300D" → 列舉值)
- ✅ 控制邏輯函數指標已設置 (7 個控制邏輯模組)
- ✅ LS300D 的 7 個控制邏輯檔案已完整實作
- ✅ 每個控制邏輯都有 `config_get()` 函數

### 待完成的工作
需要修改 3 個關鍵檔案,添加 LS300D 的 switch-case 分支:
1. `control_hardware.c` - 硬體初始化
2. `control_logic_common.c` - API 讀取路由
3. `control_logic_common.c` - API 寫入路由

## 實作順序

### 階段 1: 寄存器常量定義 (可選但建議)

**檔案**: `control_logic_register.h`
**位置**: 第 213 行附近

**修改內容**:
```c
/* 液位檢測暫存器 */
#define REG_HIGH_LEVEL_STR "HIGH_LEVEL"         // 411112 - 高液位檢測
#define REG_MID_LEVEL_STR "MID_LEVEL"           // 411016 - 中液位檢測 [LS300D專用]
#define REG_LOW_LEVEL_STR "LOW_LEVEL"           // 411113 - 低液位檢測
#define REG_LEAK_DETECTION_STR "LEAK_DETECTION" // 411114 - 洩漏檢測
```

**說明**:
- LS300D 採用三液位設計 (高/中/低),需要中液位常量
- 目前 `control_logic_ls300d_5.c` 使用硬編碼字串 `"MID_LEVEL"`
- 添加此常量可保持代碼一致性

### 階段 2: 硬體初始化支援

**檔案**: `control_hardware.c`
**函數**: `control_hardware_init(int machine_type)`
**位置**: 第 1174-1180 行 (在 LX1400 case 後, default case 前)

**修改內容**:
```c
case CONTROL_LOGIC_MACHINE_TYPE_LS300D:
    /* LS300D 機型配置 - 雙備援感測器設計 */
    /* 配置 Port 0 的 AI/AO 模式 */
    ret |= control_hardware_AI_AO_mode_set(0, 0, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
    ret |= control_hardware_AI_AO_mode_set(0, 1, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
    ret |= control_hardware_AI_AO_mode_set(0, 2, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
    ret |= control_hardware_AI_AO_mode_set(0, 3, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
    /* 配置 Port 1 的 AI/AO 模式 */
    ret |= control_hardware_AI_AO_mode_set(1, 0, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
    ret |= control_hardware_AI_AO_mode_set(1, 1, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
    ret |= control_hardware_AI_AO_mode_set(1, 2, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
    ret |= control_hardware_AI_AO_mode_set(1, 3, AI_AO_MODE_CURRENT_OUT, 2000);
    debug(tag, "LS300D control_hardware_AI_AO_mode_set ret = %d", ret);
    break;
```

**硬體配置說明**:
- **Port 0, CH 0-3**: 4 個電流輸入通道 (4-20mA 外部供電)
- **Port 1, CH 0-2**: 3 個電流輸入通道 (4-20mA 外部供電)
- **Port 1, CH 3**: 1 個電流輸出通道 (4-20mA 輸出,用於比例閥)
- **與 LS80 相同**: LS300D 與 LS80 使用相同硬體配置,向後兼容

### 階段 3: API 讀取路由支援

**檔案**: `control_logic_common.c`
**函數**: `control_logic_api_data_append_to_json()`
**位置**: 第 607 行後 (在 LX1400 case 結束後)

**前置檢查**: 確認檔案頂部是否已 include `control_logic_ls300d.h`
```c
#include "kenmec/main_application/control_logic/ls300d/control_logic_ls300d.h"
```

**修改內容**:
```c
case CONTROL_LOGIC_MACHINE_TYPE_LS300D: {
    switch (logic_id) {
        case 1:
            control_logic_ls300d_1_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_data_append_to_json(json_root, register_list, list_size);
            break;
        case 2:
            control_logic_ls300d_2_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_data_append_to_json(json_root, register_list, list_size);
            break;
        case 3:
            control_logic_ls300d_3_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_data_append_to_json(json_root, register_list, list_size);
            break;
        case 4:
            control_logic_ls300d_4_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_data_append_to_json(json_root, register_list, list_size);
            break;
        case 5:
            control_logic_ls300d_5_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_data_append_to_json(json_root, register_list, list_size);
            break;
        case 6:
            control_logic_ls300d_6_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_data_append_to_json(json_root, register_list, list_size);
            break;
        case 7:
            control_logic_ls300d_7_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_data_append_to_json(json_root, register_list, list_size);
            break;
        default:
            break;
    }
    break;
}
```

**API 路由說明**:
- Logic 1: 溫度控制 (雙備援溫度感測器)
- Logic 2: 壓力控制 (雙備援壓力感測器)
- Logic 3: 流量控制
- Logic 4: 泵浦監控
- Logic 5: 水泵控制 (三液位設計)
- Logic 6: 閥門控制
- Logic 7: 雙直流泵浦控制

### 階段 4: API 寫入路由支援

**檔案**: `control_logic_common.c`
**函數**: `control_logic_api_write_by_json()`
**位置**: 第 698 行後 (在 LX1400 case 結束後)

**修改內容**:
```c
case CONTROL_LOGIC_MACHINE_TYPE_LS300D: {
    switch (logic_id) {
        case 1:
            control_logic_ls300d_1_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
            break;
        case 2:
            control_logic_ls300d_2_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
            break;
        case 3:
            control_logic_ls300d_3_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
            break;
        case 4:
            control_logic_ls300d_4_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
            break;
        case 5:
            control_logic_ls300d_5_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
            break;
        case 6:
            control_logic_ls300d_6_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
            break;
        case 7:
            control_logic_ls300d_7_config_get(&list_size, &register_list, &file_path);
            ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
            break;
        default:
            break;
    }
    break;
}
```

**配置檔案持久化**:
- `/usrdata/register_configs_ls300d_1.json` (溫度控制)
- `/usrdata/register_configs_ls300d_2.json` (壓力控制)
- `/usrdata/register_configs_ls300d_3.json` (流量控制)
- `/usrdata/register_configs_ls300d_4.json` (泵浦監控)
- `/usrdata/register_configs_ls300d_5.json` (水泵控制)
- `/usrdata/register_configs_ls300d_6.json` (閥門控制)
- `/usrdata/register_configs_ls300d_7.json` (雙直流泵浦)

## 測試策略

### 1. 編譯驗證
```bash
cd /home/stephen/dk-ipc/SDK_Kenmec_CDU/application_dexatek
make clean && make
```

預期結果: 編譯成功,無錯誤或警告

### 2. 功能測試

**硬體初始化測試**:
```bash
# 設置 LS300D 機型
echo '{"machine_type": "LS300D"}' > /usrdata/system_config.json

# 啟動應用並檢查日誌
./build/application_dexatek | grep "LS300D control_hardware_AI_AO_mode_set"

# 預期輸出: ret = 0
```

**API 讀取測試**:
```bash
# 測試溫度控制 (Logic 1)
curl -X GET "http://localhost:8080/api/control_logic/1/data"

# 預期: 返回 JSON 包含溫度相關寄存器
```

**API 寫入測試**:
```bash
# 測試壓力控制 (Logic 2)
curl -X POST "http://localhost:8080/api/control_logic/2/data" \
     -H "Content-Type: application/json" \
     -d '{"PRESSURE_SETPOINT": 350}'

# 預期: 返回 {"status": "success"}
```

### 3. 回歸測試

確保 LS80 和 LX1400 機種不受影響:
```bash
# 測試 LS80
echo '{"machine_type": "LS80"}' > /usrdata/system_config.json
./build/application_dexatek
curl -X GET "http://localhost:8080/api/control_logic/1/data"

# 測試 LX1400
echo '{"machine_type": "LX1400"}' > /usrdata/system_config.json
./build/application_dexatek
curl -X GET "http://localhost:8080/api/control_logic/1/data"
```

## 風險評估

**風險等級**: 極低

| 風險項目 | 緩解措施 |
|---------|---------|
| 破壞現有機種功能 | 只添加新 case,不修改現有邏輯 |
| 硬體配置錯誤 | 使用與 LS80 相同的驗證配置 |
| API 路由錯誤 | 使用與 LS80/LX1400 相同模式 |

## 關鍵檔案清單

需要修改的檔案:
1. `kenmec/main_application/control_logic/control_logic_register.h` (可選)
2. `kenmec/main_application/control_logic/control_hardware.c` (必須)
3. `kenmec/main_application/control_logic/control_logic_common.c` (必須,兩處)

參考檔案:
1. `kenmec/main_application/control_logic/ls300d/control_logic_ls300d.h` (驗證函數宣告)
2. `kenmec/main_application/control_logic/control_logic_config.h` (機種列舉定義)

## 實作檢查清單

- [ ] 階段 1: 添加 REG_MID_LEVEL_STR 到 control_logic_register.h
- [ ] 階段 2: 添加 LS300D 硬體初始化到 control_hardware.c
- [ ] 階段 3: 添加 LS300D API 讀取路由到 control_logic_common.c
- [ ] 階段 4: 添加 LS300D API 寫入路由到 control_logic_common.c
- [ ] 編譯測試通過
- [ ] 硬體初始化測試通過
- [ ] API 讀取測試通過 (Logic 1-7)
- [ ] API 寫入測試通過 (Logic 1-7)
- [ ] LS80 回歸測試通過
- [ ] LX1400 回歸測試通過

## 預估時間

總計: 1-2 小時 (包含測試)
- 階段 1: 5 分鐘
- 階段 2: 15 分鐘
- 階段 3: 20 分鐘
- 階段 4: 20 分鐘
- 測試: 30-60 分鐘

## 成功標準

1. ✅ 所有階段編譯成功
2. ✅ LS300D 硬體正確初始化
3. ✅ LS300D 的 7 個控制邏輯都可透過 API 讀寫
4. ✅ LS80 和 LX1400 功能正常
5. ✅ 無編譯警告或錯誤

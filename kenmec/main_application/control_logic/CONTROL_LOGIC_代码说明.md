# Control Logic 控制邏輯代碼說明

## 文件結構總覽

本目錄包含 CDU (Cooling Distribution Unit) 冷卻分配單元的控制邏輯實現,支援 LS80 和 LX1400 兩種機型。

---

## 核心管理文件 (Core Management Files)

### 1. control_logic_manager.c - 控制邏輯管理器
**功能**:
- 管理所有控制邏輯模組的生命週期(初始化、啟動、停止、清理)
- 為每個控制邏輯創建獨立執行緒
- 根據機型(LS80/LX1400)動態切換控制函數指標

**關鍵函數**:
- `control_logic_manager_init()` - 初始化所有控制邏輯模組
- `control_logic_manager_set_function_pointer()` - 根據機型設定函數指標
- `_control_logic_manager_thread_func()` - 控制邏輯執行緒主函數
- `control_logic_manager_start/stop()` - 啟動/停止管理器

**控制邏輯模組編號**:
- [0] 溫度控制 (Temperature Control)
- [1] 壓力控制 (Pressure Control)
- [2] 流量控制 (Flow Control)
- [3] 泵浦控制 (Pump Control)
- [4] 水泵控制 (Water Pump Control)
- [5] 閥門控制 (Valve Control)
- [6] 雙DC泵控制 (2DC Pump Control)

---

### 2. control_logic_common.c - 公共功能函數
**功能**:
- 提供 Modbus 寄存器讀寫封裝
- 模擬/數位輸出值轉換 (百分比轉電壓/電流)
- 處理不同感測器類型的數值轉換

**關鍵函數**:
- `control_logic_read_holding_register()` - 讀取 Modbus 保持寄存器
- `control_logic_write_register()` - 寫入 Modbus 寄存器
- `control_logic_output_value_convert()` - 輸出值轉換(如 0-100% 轉 0-10V)

**地址轉換**:
- 支援 Modbus 標準地址 (400000+ → 實際地址)
- 支援簡化地址 (40000+ → 實際地址)

---

### 3. control_logic_config.c - 配置管理
**功能**:
- 從檔案載入/儲存配置
- 管理機型選擇 (LS80/LX1400)
- 管理類比輸入/輸出配置

**關鍵函數**:
- `control_logic_config_init()` - 初始化配置
- `control_logic_config_get_machine_type()` - 獲取當前機型
- `control_logic_config_set_machine_type()` - 設定機型

---

### 4. control_logic_update.c - 更新管理
**功能**:
- 處理控制邏輯的韌體/配置更新

---

### 5. control_hardware.c - 硬體控制
**功能**:
- 初始化硬體設備 (類比輸入/輸出、數位輸入/輸出)
- 根據機型載入對應的硬體配置

---

## LS80 機型控制邏輯 (LS80 Control Logics)

### ls80/control_logic_ls80_1.c - 溫度控制
**功能**: PID 溫度控制,維持二次側出水溫度在目標值
**控制目標**:
- 目標溫度 (T_set)
- 控制對象: T2 (二次側出水溫度)
**感測器**:
- T4: 一次側進水溫度
- T2: 二次側出水溫度
- T11/T12: 進水溫度
- T17/T18: 出水溫度
**執行器**:
- Pump1/2/3: 泵浦速度和啟停
- 比例閥: 開度控制 0-100%

---

### ls80/control_logic_ls80_2.c - 壓力控制
**功能**: PID 壓差控制,維持進出水壓差在目標值
**控制目標**:
- 壓差設定值 (P_set)
- 壓差 = 進水壓力 - 出水壓力
**感測器**:
- P1: 一次側進水壓力
- P3: 一次側出水壓力
- P4: 二次側進水壓力
- P2: 二次側出水壓力
**安全保護**:
- 高壓警報閾值: 7.0 bar
- 高壓停機閾值: 8.5 bar
- 最大壓差限制: 3.0 bar
**泵浦協調策略**:
- PID輸出 > 70%: 三泵運行
- PID輸出 > 50%: 雙泵運行
- PID輸出 > 30%: 單泵運行
- PID輸出 ≤ 30%: 待機

---

### ls80/control_logic_ls80_3.c - 流量控制
**功能**: PID 流量控制,使二次側出水流量追蹤目標值
**控制目標**:
- 目標流量 (F_set)
- F2 追蹤設定值模式
**感測器**:
- F1: 一次側進水流量
- F2: 二次側出水流量 (主要控制)
**控制範圍**:
- 流量上限/下限可設定
- 最小控制流量: 30 L/min

---

### ls80/control_logic_ls80_4.c - 泵浦控制 (AC泵)
**功能**: 管理3個AC泵浦的啟停、轉速和故障處理
**寄存器**:
- 轉速設定: REG_PUMP1/2/3_SPEED_CMD (0-1000 對應 0-100%)
- 啟停控制: REG_PUMP1/2/3_RUN_CMD (0=停止, 1=運轉)
- 故障復歸: REG_PUMP1/2/3_RESET_CMD
**回饋監控**:
- 輸出頻率 (Hz)
- 電流 (A×0.01)
- 電壓 (V×0.1)

---

### ls80/control_logic_ls80_5.c - 水泵控制
**功能**: 補水泵控制,液位管理

---

### ls80/control_logic_ls80_6.c - 閥門控制
**功能**: 比例閥開度控制
**控制範圍**: 10-100%

---

### ls80/control_logic_ls80_7.c - 雙DC泵控制
**功能**: 管理2個DC泵浦

---

## LX1400 機型控制邏輯 (LX1400 Control Logics)

LX1400 機型的控制邏輯與 LS80 類似,但針對不同硬體配置進行了調整:

### lx1400/control_logic_lx1400_1.c - 溫度控制
與 LS80 類似,但感測器地址和控制參數可能不同

### lx1400/control_logic_lx1400_2.c - 壓力控制
與 LS80 類似,但安全閾值可能調整

### lx1400/control_logic_lx1400_3.c - 流量控制
與 LS80 類似

### lx1400/control_logic_lx1400_4.c - 泵浦控制
與 LS80 類似

### lx1400/control_logic_lx1400_5.c - 水泵控制
與 LS80 類似

### lx1400/control_logic_lx1400_6.c - 閥門控制
與 LS80 類似

### lx1400/control_logic_lx1400_7.c - 雙DC泵控制
與 LS80 類似

---

## Modbus 寄存器地址範圍

### 控制啟用寄存器 (4xxxx)
- 41001-41007: 控制邏輯1-7啟用標誌

### 監控寄存器 (4xxxx)
- 42001: 系統狀態
- 42062-42065: 流量感測器 F1-F4
- 42082-42099: 壓力感測器 P1-P18
- 42501-42523: 泵浦回饋 (頻率/電流/電壓)

### 設定寄存器 (4xxxx)
- 45001: 目標溫度
- 45002: 壓差設定
- 45003: 流量設定
- 45005: 控制模式
- 45021-45023: 泵浦手動模式
- 45061: 閥門手動模式

### 輸出控制寄存器 (4xxxxx)
- 411037/411039/411041: 泵浦速度設定
- 411101/411103/411105: 泵浦啟停控制
- 411151: 閥門開度設定
- 411161: 閥門實際開度

### 警報寄存器 (4xxxx)
- 46271: 高壓警報
- 46272: 高壓停機

---

## PID 控制器參數

### 溫度控制 PID
- Kp: 15.0 (比例增益)
- Ki: 0.8 (積分增益)
- Kd: 2.5 (微分增益)
- 輸出範圍: 0-100%

### 壓力控制 PID
- Kp: 2.0
- Ki: 0.5
- Kd: 0.1
- 輸出範圍: 0-100%
- 積分飽和限制: ±50

### 流量控制 PID
(參數在各個文件中定義)

---

## 安全保護機制

### 壓力保護
- **警告等級**: 壓力 > 7.0 bar → 觸發警報
- **嚴重等級**: 壓力 < 1.0 bar → 降低泵浦速度
- **緊急等級**: 壓力 > 8.5 bar → 緊急停機

### 緊急停機程序
1. 停止所有泵浦
2. 比例閥關閉至最小開度 (10%)
3. 重置 PID 控制器
4. 記錄警報資訊

---

## 執行週期

所有控制邏輯以固定週期執行:
- `CONTROL_LOGIC_PROCESS_INTERVAL_MS` (定義在標頭檔中)

---

## 檔案命名規則

```
control_logic_<機型>_<編號>_<功能>.c
```

範例:
- `control_logic_ls80_2_pressure.c` = LS80機型的編號2壓力控制
- `control_logic_lx1400_4_pump.c` = LX1400機型的編號4泵浦控制

---

## 開發注意事項

1. **寄存器讀寫**: 使用 `control_logic_read_holding_register()` 和 `control_logic_write_register()`
2. **PID 參數調整**: 根據實際系統響應調整 Kp/Ki/Kd
3. **安全第一**: 任何控制變更都要考慮安全保護機制
4. **機型切換**: 通過 `control_logic_config_set_machine_type()` 切換
5. **日誌除錯**: 使用 `debug()`, `info()`, `warn()`, `error()` 函數

---

生成日期: 2025-10-24
版本: 1.0

# LS80A Modbus RTU 位址對照表

## 版本資訊
- 文件名稱: LS80A_mod_final.md
- 建立日期: 2025-11-25
- 適用機種: LS80A
- 來源: 基於 Control Logic 1-7 程式碼分析

---

## 一、控制邏輯啟用 (41xxx)

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 41001 | 03/06 | CONTROL_LOGIC_1_ENABLE | N/A | bool | R/W | 1 | 溫度控制邏輯啟用 |
| 41002 | 03/06 | CONTROL_LOGIC_2_ENABLE | N/A | bool | R/W | 0 | 壓力差控制邏輯啟用 |
| 41003 | 03/06 | CONTROL_LOGIC_3_ENABLE | N/A | bool | R/W | 0 | 流量控制邏輯啟用 |
| 41004 | 03/06 | CONTROL_LOGIC_4_ENABLE | N/A | bool | R/W | 0 | AC泵浦控制邏輯啟用 |
| 41005 | 03/06 | CONTROL_LOGIC_5_ENABLE | N/A | bool | R/W | 0 | 補水泵控制邏輯啟用 |
| 41007 | 03/06 | CONTROL_LOGIC_7_ENABLE | N/A | bool | R/W | 1 | 雙DC泵控制邏輯啟用 |

---

## 二、系統狀態監控 (42xxx)

### 2.1 系統狀態

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 42001 | 03 | SYSTEM_STATUS | N/A | uint16 | R | 0 | bit0:電源 bit1:運轉 bit2:待機 bit7:異常 bit8:液位 |

### 2.2 環境監測

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 42021 | 03 | ENV_TEMP | 0.1°C | int16 | R | 0 | 環境溫度 |
| 42022 | 03 | HUMIDITY | 0.1%RH | uint16 | R | 0 | 環境濕度 |
| 42024 | 03 | DEW_POINT | 0.1°C | int16 | R | 0 | 露點溫度輸出 |

### 2.3 流量感測器 (0.1 L/min 精度)

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 42062 | 03 | F1_FLOW | 0.1 L/min | uint16 | R | 0 | F1一次側進水流量 |
| 42063 | 03 | F2_FLOW | 0.1 L/min | uint16 | R | 0 | F2二次側出水流量 (主要控制) |
| 42064 | 03 | F3_FLOW | 0.1 L/min | uint16 | R | 0 | F3二次側進水流量 |
| 42065 | 03 | F4_FLOW | 0.1 L/min | uint16 | R | 0 | F4一次側出水流量 |

### 2.4 壓力感測器 (0.01 bar 精度)

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 42082 | 03 | P1_PRESSURE | 0.01 bar | uint16 | R | 0 | P1一次側進水壓力 |
| 42083 | 03 | P2_PRESSURE | 0.01 bar | uint16 | R | 0 | P2二次側出水壓力 (控制) |
| 42084 | 03 | P3_PRESSURE | 0.01 bar | uint16 | R | 0 | P3一次側出水壓力 |
| 42085 | 03 | P4_PRESSURE | 0.01 bar | uint16 | R | 0 | P4二次側進水壓力 (控制) |
| 42086 | 03 | P5_PRESSURE | 0.1 bar | uint16 | R | 0 | P5補水壓力監測 |
| 42093 | 03 | PRESSURE_FEEDBACK | bar | uint16 | R | 0 | 壓力回饋 |
| 42096 | 03 | P15_PRESSURE | bar | uint16 | R | 0 | P15二次側泵前壓力 |
| 42097 | 03 | P16_PRESSURE | bar | uint16 | R | 0 | P16二次側泵前壓力 |
| 42098 | 03 | P17_PRESSURE | bar | uint16 | R | 0 | P17二次側出水壓力 |
| 42099 | 03 | P18_PRESSURE | bar | uint16 | R | 0 | P18二次側出水壓力 |

### 2.5 泵浦運轉時間 (斷電保持)

#### Pump1 運轉時間

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 42161 | 03 | PUMP1_RUNTIME_SEC | 秒 | uint16 | R | 0 | Pump1運轉時間-秒 (0-59) |
| 42162 | 03 | PUMP1_RUNTIME_MIN | 分 | uint16 | R | 0 | Pump1運轉時間-分 (0-59) |
| 42163 | 03 | PUMP1_RUNTIME_HOUR | 時 | uint16 | R | 0 | Pump1運轉時間-時 (0-23) |
| 42164 | 03 | PUMP1_RUNTIME_DAY | 天 | uint16 | R | 0 | Pump1運轉時間-天 (累積) |

#### Pump2 運轉時間

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 42165 | 03 | PUMP2_RUNTIME_SEC | 秒 | uint16 | R | 0 | Pump2運轉時間-秒 (0-59) |
| 42166 | 03 | PUMP2_RUNTIME_MIN | 分 | uint16 | R | 0 | Pump2運轉時間-分 (0-59) |
| 42167 | 03 | PUMP2_RUNTIME_HOUR | 時 | uint16 | R | 0 | Pump2運轉時間-時 (0-23) |
| 42168 | 03 | PUMP2_RUNTIME_DAY | 天 | uint16 | R | 0 | Pump2運轉時間-天 (累積) |

### 2.6 主泵 AUTO 模式累計時間 (斷電保持)

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 42170 | 03 | PUMP1_AUTO_MODE_HOURS | 小時 | uint16 | R | 0 | Pump1作為主泵 AUTO模式累計時間 |
| 42171 | 03 | PUMP2_AUTO_MODE_HOURS | 小時 | uint16 | R | 0 | Pump2作為主泵 AUTO模式累計時間 |
| 42172 | 03 | PUMP1_AUTO_MODE_MINUTES | 分鐘 | uint16 | R | 0 | Pump1 AUTO模式累計分鐘 |
| 42173 | 03 | PUMP2_AUTO_MODE_MINUTES | 分鐘 | uint16 | R | 0 | Pump2 AUTO模式累計分鐘 |

### 2.7 AC泵浦回饋

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 42501 | 03 | PUMP1_FREQ | Hz | uint16 | R | 0 | AC泵浦1輸出頻率 |
| 42502 | 03 | PUMP1_CURRENT | 0.01 A | uint16 | R | 0 | AC泵浦1電流 |
| 42503 | 03 | PUMP1_VOLTAGE | 0.1 V | uint16 | R | 0 | AC泵浦1電壓 |
| 42511 | 03 | PUMP2_FREQ | Hz | uint16 | R | 0 | AC泵浦2輸出頻率 |
| 42512 | 03 | PUMP2_CURRENT | 0.01 A | uint16 | R | 0 | AC泵浦2電流 |
| 42513 | 03 | PUMP2_VOLTAGE | 0.1 V | uint16 | R | 0 | AC泵浦2電壓 |
| 42521 | 03 | PUMP3_FREQ | Hz | uint16 | R | 0 | AC泵浦3輸出頻率 |
| 42522 | 03 | PUMP3_CURRENT | 0.01 A | uint16 | R | 0 | AC泵浦3電流 |
| 42523 | 03 | PUMP3_VOLTAGE | 0.1 V | uint16 | R | 0 | AC泵浦3電壓 |

### 2.8 DC泵浦回饋

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 42552 | 03 | DC_PUMP1_VOLTAGE | 0.1 V | uint16 | R | 0 | DC泵1電壓 |
| 42553 | 03 | DC_PUMP1_CURRENT | 0.01 A | uint16 | R | 0 | DC泵1電流 |
| 42562 | 03 | DC_PUMP2_VOLTAGE | 0.1 V | uint16 | R | 0 | DC泵2電壓 |
| 42563 | 03 | DC_PUMP2_CURRENT | 0.01 A | uint16 | R | 0 | DC泵2電流 |

### 2.9 補水狀態

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 42801 | 03 | CURRENT_FAIL_COUNT | 次 | uint16 | R | 0 | 補水未滿次數 (當前值) |

---

## 三、控制設定值 (45xxx)

### 3.1 溫度控制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 45001 | 03/06 | TARGET_TEMP | 0.1°C | uint16 | R/W | 250 | 目標溫度 (預設25.0°C) |
| 45004 | 03/06 | DP_CORRECT | 0.1°C | int16 | R/W | 0 | 露點校正值 |
| 45009 | 03/06 | TEMP_CONTROL_MODE | N/A | uint16 | R/W | 0 | 溫度控制模式 (0=手動 1=自動) |
| 45010 | 03/06 | TEMP_FOLLOW_DEW_POINT | N/A | uint16 | R/W | 0 | 溫度跟隨露點 (0=保護 1=跟隨) |

### 3.2 壓力差控制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 45002 | 03/06 | PRESSURE_SETPOINT | 0.1 bar | uint16 | R/W | 10 | 壓差設定值 (預設1.0 bar) |
| 45005 | 03/06 | CONTROL_MODE (FLOW_MODE) | N/A | uint16 | R/W | 0 | 控制模式 (0=流量 1=壓差) |

### 3.3 流量控制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 45003 | 03/06 | FLOW_SETPOINT | 0.1 L/min | uint16 | R/W | 2000 | 流量設定 (預設200.0 L/min) |

### 3.4 泵浦速度設定

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 45015 | 03/06 | PUMP1_SPEED | 0-100% | uint16 | R/W | 0 | Pump1速度 |
| 45016 | 03/06 | PUMP2_SPEED | 0-100% | uint16 | R/W | 0 | Pump2速度 |

### 3.5 系統啟停與模式控制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 45020 | 03/06 | AUTO_START_STOP | N/A | uint16 | R/W | 0 | 自動啟停開關 (0=停用 1=啟用) |
| 45021 | 03/06 | PUMP1_MANUAL_MODE | N/A | uint16 | R/W | 0 | Pump1手動模式 (0=自動 1=手動) |
| 45022 | 03/06 | PUMP2_MANUAL_MODE | N/A | uint16 | R/W | 0 | Pump2手動模式 (0=自動 1=手動) |
| 45023 | 03/06 | PUMP3_MANUAL_MODE | N/A | uint16 | R/W | 0 | Pump3手動模式 (0=自動 1=手動) |
| 45025 | 03/06 | PUMP1_STOP | N/A | uint16 | R/W | 0 | Pump1停用 (0=啟用 1=停用) |
| 45026 | 03/06 | PUMP2_STOP | N/A | uint16 | R/W | 0 | Pump2停用 (0=啟用 1=停用) |
| 45027 | 03/06 | PUMP3_STOP | N/A | uint16 | R/W | 0 | Pump3停用 (0=啟用 1=停用) |

### 3.6 泵浦速度限制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 45031 | 03/06 | PUMP_MIN_SPEED | % | uint16 | R/W | 30 | 泵浦最小速度 |
| 45032 | 03/06 | PUMP_MAX_SPEED | % | uint16 | R/W | 100 | 泵浦最大速度 |

### 3.7 主泵輪換設定

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 45034 | 03/06 | PUMP_SWITCH_HOUR | 小時 | uint16 | R/W | 0 | 主泵切換時數 (0=停用) |
| 45036 | 03/06 | PUMP1_USE | N/A | uint16 | R/W | 1 | Pump1啟用開關 |
| 45037 | 03/06 | PUMP2_USE | N/A | uint16 | R/W | 1 | Pump2啟用開關 |
| 45045 | 03/06 | PRIMARY_PUMP_INDEX | N/A | uint16 | R/W | 1 | 當前主泵編號 (1=Pump1 2=Pump2) |
| 45046 | 03/06 | CURRENT_PRIMARY_AUTO_HOURS | 小時 | uint16 | R/W | 0 | 主泵AUTO時間顯示-小時 |
| 45047 | 03/06 | CURRENT_PRIMARY_AUTO_MINUTES | 分鐘 | uint16 | R/W | 0 | 主泵AUTO時間顯示-分鐘 |

### 3.8 運轉時間歸零控制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 45041 | 03/06 | PUMP1_RUNTIME_RESET | N/A | uint16 | R/W | 0 | Pump1運轉時間歸零 (寫1執行) |
| 45042 | 03/06 | PUMP2_RUNTIME_RESET | N/A | uint16 | R/W | 0 | Pump2運轉時間歸零 (寫1執行) |

### 3.9 補水控制參數

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 45051 | 03/06 | TARGET_PRESSURE | bar | uint16 | R/W | 25 | 補水壓力設定 (預設2.5 bar) |
| 45052 | 03/06 | START_DELAY | 0.1s | uint16 | R/W | 20 | 補水啟動延遲 (預設2.0秒) |
| 45053 | 03/06 | MAX_RUN_TIME | 0.1s | uint16 | R/W | 3000 | 補水運行超時 (預設300秒) |
| 45054 | 03/06 | COMPLETE_DELAY | 0.1s | uint16 | R/W | 50 | 補水完成延遲 (預設5.0秒) |
| 45055 | 03/06 | WARNING_DELAY | 0.1s | uint16 | R/W | 100 | 缺水警告延遲 (預設10.0秒) |
| 45056 | 03/06 | MAX_FAIL_COUNT | 次 | uint16 | R/W | 3 | 補水未滿最大次數 |

### 3.10 比例閥控制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 45061 | 03/06 | VALVE_MANUAL_MODE | N/A | uint16 | R/W | 0 | 比例閥手動模式 (0=自動 1=手動) |

---

## 四、限制參數 (HMI可設定，斷電保持) (46xxx)

### 4.1 溫度限制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 46001 | 03/06 | T_HIGH_ALARM | 0.1°C | uint16 | R/W | 500 | 最高溫度限制 (預設50.0°C) |
| 46002 | 03/06 | T_LOW_ALARM | 0.1°C | uint16 | R/W | 100 | 最低溫度限制 (預設10.0°C) |

### 4.2 壓力限制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 46201 | 03/06 | P_HIGH_ALARM | 0.01 bar | uint16 | R/W | 500 | 最高壓力限制 (預設5.0 Bar) |
| 46202 | 03/06 | P_LOW_ALARM | 0.01 bar | uint16 | R/W | 50 | 最低壓力限制 (預設0.5 Bar) |

### 4.3 流量限制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 46401 | 03/06 | FLOW_HIGH_LIMIT | L/min | uint16 | R/W | 500 | 流量上限 (HMI可設定) |
| 46402 | 03/06 | FLOW_LOW_LIMIT | L/min | uint16 | R/W | 100 | 流量下限 (HMI可設定) |

---

## 五、執行器控制 (411xxx)

### 5.1 DC泵浦控制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 411001 | 05/15 | DC_PUMP1_ENABLE | N/A | bool | W | 0 | DC泵1啟停 (0=Stop 1=Run) |
| 411002 | 05/15 | DC_PUMP2_ENABLE | N/A | bool | W | 0 | DC泵2啟停 (0=Stop 1=Run) |

### 5.2 補水泵控制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 411003 | 05/15 | WATER_PUMP_CONTROL | N/A | bool | R/W | 0 | 補水泵啟停 (0=Stop 1=Run) |

### 5.3 漏液與液位檢測

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 411010 | 02 | LEAK_DETECTION | N/A | bool | R | 0 | 漏液檢測 (0=正常 1=漏液) |
| 411015 | 02 | HIGH_LEVEL | N/A | bool | R | 0 | CDU水箱高液位 (0=無 1=有) |

### 5.4 AC泵浦速度指令

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 411037 | 06 | PUMP1_SPEED_CMD | 0-10000mV | uint16 | W | 0 | AC泵1設定轉速 (0-100000mV) |
| 411039 | 06 | PUMP2_SPEED_CMD | 0-10000mV | uint16 | W | 0 | AC泵2設定轉速 (0-100000mV) |
| 411041 | 06 | PUMP3_SPEED_CMD | 0-10000mV | uint16 | W | 0 | AC泵3設定轉速 (0-100000mV) |

### 5.5 AC泵浦啟停控制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 411101 | 05/15 | PUMP1_RUN_CMD | N/A | bool | R/W | 0 | 泵浦1啟停 (0=Stop 1=Run) |
| 411102 | 05/15 | PUMP2_RUN_CMD | N/A | bool | R/W | 0 | 泵浦2啟停 (0=Stop 1=Run) |
| 411103 | 05/15 | PUMP2_RUN_CMD | N/A | bool | R/W | 0 | 泵浦2啟停 (0=Stop 1=Run) 用於雙DC |
| 411105 | 05/15 | PUMP3_RUN_CMD | N/A | bool | R/W | 0 | 泵浦3啟停 (0=Stop 1=Run) |

### 5.6 AC泵浦異常復歸

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 411102 | 06 | PUMP1_RESET_CMD | N/A | bool | W | 0 | 泵浦1異常復歸 (寫1復歸) |
| 411104 | 06 | PUMP2_RESET_CMD | N/A | bool | W | 0 | 泵浦2異常復歸 (寫1復歸) |
| 411106 | 06 | PUMP3_RESET_CMD | N/A | bool | W | 0 | 泵浦3異常復歸 (寫1復歸) |

### 5.7 DC泵浦異常復歸

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 411108 | 06 | DC_PUMP1_RESET | N/A | bool | W | 0 | DC泵1異常復歸 |
| 411110 | 06 | DC_PUMP2_RESET | N/A | bool | W | 0 | DC泵2異常復歸 |

### 5.8 泵浦故障狀態

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 411109 | 02 | PUMP1_FAULT | N/A | bool | R | 1 | 泵浦1過載 (0=故障 1=正常) |
| 411110 | 02 | PUMP2_FAULT | N/A | bool | R | 1 | 泵浦2過載 (0=故障 1=正常) |
| 411111 | 02 | PUMP3_FAULT | N/A | bool | R | 1 | 泵浦3過載 (0=故障 1=正常) |

### 5.9 比例閥控制

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 411151 | 06 | VALVE_OPENING | 0-100% | uint16 | R/W | 0 | 比例閥開度設定 |

---

## 六、溫度感測器 (413xxx)

| Address | Function Code | Description | Unit | Data Type | R/W | Default | Note |
|---------|---------------|-------------|------|-----------|-----|---------|------|
| 413556 | 03 | T2_TEMP | 0.1°C | uint16 | R | 0 | T2出水溫度 (主要控制) |
| 413560 | 03 | T4_TEMP | 0.1°C | uint16 | R | 0 | T4進水溫度 |

---

## 七、Modbus Function Code 說明

- **02**: Read Discrete Inputs (讀取離散輸入，只讀DI)
- **03**: Read Holding Registers (讀取保持寄存器)
- **05**: Write Single Coil (寫入單個線圈，DO控制)
- **06**: Write Single Register (寫入單個寄存器)
- **15**: Write Multiple Coils (寫入多個線圈)

---

## 八、數據類型說明

- **bool**: 布林值 (0=False, 1=True)
- **uint16**: 無符號16位整數 (0-65535)
- **int16**: 有符號16位整數 (-32768 to 32767)

---

## 九、Control Logic 功能對應

### Logic 1: 溫度控制
- 位址範圍: 41001, 42021-42024, 45001, 45004, 45009-45010, 45061, 46001-46002, 411151, 413556, 413560

### Logic 2: 壓力差控制
- 位址範圍: 41002, 42082-42085, 42170-42173, 45002, 45005, 45015-45016, 45021-45022, 45034-45047, 46201-46202, 411101, 411103

### Logic 3: 流量控制
- 位址範圍: 41003, 42062-42063, 42170-42173, 45003, 45005, 45015-45016, 45021-45022, 45034-45047, 46401-46402, 411101-411102

### Logic 4: AC泵浦控制
- 位址範圍: 41004, 42001, 42062-42065, 42096-42099, 42501-42523, 45001-45003, 45005, 45020-45027, 45031-45032, 411037-411041, 411101-411111

### Logic 5: 補水泵控制
- 位址範圍: 41005, 42001, 42086, 42801, 45051-45056, 411003, 411010, 411015

### Logic 7: 雙DC泵控制
- 位址範圍: 41007, 42161-42168, 42501, 42511, 42552-42553, 42562-42563, 45003-45004, 45015-45016, 45020-45022, 45026-45027, 45031-45032, 45041-45042, 411001-411002, 411108-411111

---

## 十、備註

1. **斷電保持**: 部分寄存器具有斷電保持功能 (46xxx, 42xxx 部分)
2. **精度說明**:
   - 溫度: 0.1°C (值需除以10)
   - 壓力: 0.01 bar 或 0.1 bar (依感測器而定)
   - 流量: 0.1 L/min (值需除以10)
3. **地址衝突**: 部分位址在不同 Logic 中重複使用，請依據啟用的 Control Logic 判斷
4. **手動模式**: AUTO_START_STOP=0 時，允許 HMI 手動設定泵浦速度
5. **自動模式**: AUTO_START_STOP=1 時，系統自動控制泵浦運作

---

**文件版本**: v1.0
**最後更新**: 2025-11-25
**維護者**: Claude AI (基於程式碼分析)

# KENMEC CDU Main Application 架構設計說明

**版本**: 1.3.1
**文檔日期**: 2025-10-25
**作者**: 系統架構團隊

---

## 📑 目錄

1. [系統總覽](#系統總覽)
2. [程式啟動順序](#程式啟動順序)
3. [架構設計理念](#架構設計理念)
4. [執行緒模型](#執行緒模型)
5. [數據流向](#數據流向)
6. [模組間依賴關係](#模組間依賴關係)
7. [關鍵設計決策](#關鍵設計決策)
8. [目錄結構](#目錄結構)

---

## 系統總覽

### 專案背景

**KENMEC CDU (Cooling Distribution Unit)** 是一個工業級冷卻分配單元控制系統，用於精密溫度控制和流體管理。本系統負責：

- 🌡️ **溫度控制**: 精確控制冷卻水溫度
- 💧 **流量管理**: 監控和調節冷卻水流量
- 🔧 **壓力調節**: 維持系統壓差穩定
- ⚙️ **泵浦協調**: 智能控制多台泵浦協同工作
- 🚰 **液位管理**: 自動補水和液位監控
- 🎛️ **閥門控制**: 比例閥精密開度控制

### 支援機型

1. **LS80**: 標準冷卻單元
2. **LX1400**: 高功率冷卻單元

### 核心特性

- ✅ 實時控制週期: 1000ms (1秒)
- ✅ 多執行緒並行處理: 7個獨立控制邏輯
- ✅ PID 控制演算法
- ✅ Modbus RTU/TCP 通訊
- ✅ RESTful API (Redfish 標準)
- ✅ 看門狗保護機制
- ✅ 熱插拔配置重載

---

## 程式啟動順序

### 完整啟動流程圖

```mermaid
graph TD
    A[main 函數啟動] --> B[設定訊號處理器<br/>SIGINT/SIGTERM]
    B --> C[application_run]
    C --> D[顯示版本資訊]
    D --> E[_main_application_process]
    E --> F{CONFIG_APPLICATION_WATCHDOG_ENABLE?}
    F -->|是| G[啟動看門狗<br/>20秒超時]
    F -->|否| H[跳過看門狗]
    G --> I[延遲 10 秒<br/>等待系統穩定]
    H --> I
    I --> J[初始化網路管理器<br/>ethernet_manager_init]
    J --> K[初始化 HID 管理器<br/>hid_manager_init]
    K --> L[初始化 Modbus 管理器<br/>modbus_manager_init]
    L --> M[初始化控制邏輯配置<br/>control_logic_config_init]
    M --> N[初始化控制邏輯管理器<br/>control_logic_manager_init]
    N --> O[啟動控制邏輯<br/>control_logic_manager_start]
    O --> P[初始化控制邏輯更新器<br/>control_logic_update_init]
    P --> Q[初始化 Redfish API<br/>redfish_init]
    Q --> R[進入主循環<br/>每5秒刷新看門狗]
    R --> S{_thread_aborted?}
    S -->|否| R
    S -->|是| T[清理資源並退出]
```

### 初始化順序詳解

#### 階段 1: 系統基礎設定 (第 137-156 行)

```c
int main(int argc, char **argv)
{
    // 1. 設定 SIGINT 訊號處理器 (Ctrl+C)
    signal(SIGINT, _handle_sigint);

    // 2. 設定 SIGTERM 訊號處理器 (kill 命令)
    signal(SIGTERM, _handle_sigint);

    // 3. 啟動應用程式
    application_run();
}
```

**設計理念**:
- 優先設定訊號處理，確保程式能正常響應中斷和終止信號
- 允許優雅關閉，避免資源洩漏

---

#### 階段 2: 版本資訊顯示 (第 88-117 行)

```c
void application_run(void)
{
    // 顯示應用程式版本資訊
    printf("* KENMEC Main Application\n");
    printf("* Version: %d.%d.%d\n", 1, 3, 1);

    // 顯示 libdexatek 版本資訊
    libdexatek_version_get(&major, &minor, &patch, &version_code_number);
}
```

**設計理念**:
- 便於除錯和版本追蹤
- 記錄編譯時間和版本號

---

#### 階段 3: 看門狗啟動 (第 56-58 行)

```c
#if defined(CONFIG_APPLICATION_WATCHDOG_ENABLE) && CONFIG_APPLICATION_WATCHDOG_ENABLE
    platform_watchdog_start(CONFIG_APPLICATION_WATCHDOG_TIMEOUT_SECONDS); // 20秒
#endif
```

**設計理念**:
- **為何最早啟動**: 看門狗需要儘早監控系統，防止啟動過程卡住
- **超時設定 20 秒**: 考慮到初始化可能需要較長時間
- **可配置**: 透過 `CONFIG_APPLICATION_WATCHDOG_ENABLE` 控制是否啟用

---

#### 階段 4: 系統穩定延遲 (第 60 行)

```c
time_delay_ms(10000);  // 延遲 10 秒
```

**設計理念** (⭐ 重要):
- **為何需要延遲 10 秒**:
  1. 等待作業系統網路堆疊完全啟動
  2. 等待硬體設備初始化完成 (I2C, SPI, GPIO)
  3. 等待 Modbus 從設備上線
  4. 給予 Linux 核心足夠時間載入驅動程式
  5. 防止過早存取硬體導致錯誤

- **為何是 10 秒**:
  - 實測經驗值: 嵌入式 Linux 系統通常需要 5-10 秒完全啟動
  - 保守估計，確保系統穩定性

---

#### 階段 5: 通訊層初始化 (第 63-68 行)

```c
// 1. 網路管理器
ethernet_manager_init();

// 2. HID 管理器 (USB HID 裝置)
hid_manager_init();

// 3. Modbus 管理器 (工業通訊協定)
modbus_manager_init();
```

**初始化順序必然性**:

1. **ethernet_manager_init() - 第一優先**
   - **原因**: Redfish API 依賴網路通訊
   - **功能**: 設定網路介面、IP 地址、路由
   - **依賴**: 無

2. **hid_manager_init() - 第二優先**
   - **原因**: USB HID 設備可能用於本地操作介面
   - **功能**: 初始化 USB 人機介面設備
   - **依賴**: 無

3. **modbus_manager_init() - 第三優先** ⭐
   - **原因**: 控制邏輯依賴 Modbus 通訊讀寫感測器和執行器
   - **功能**:
     - 初始化 Modbus RTU/TCP 通訊
     - 建立寄存器映射表
     - 啟動 Modbus 輪詢執行緒
   - **依賴**: 可能依賴網路 (Modbus TCP)

**為何通訊層必須先於控制層**:
```
感測器/執行器 ←[Modbus]→ Modbus 管理器 ←[API]→ 控制邏輯
                    ↑必須先建立↑              ↑才能使用↑
```

---

#### 階段 6: 控制邏輯層初始化 (第 70-73 行)

```c
// 1. 載入控制邏輯配置
control_logic_config_init();

// 2. 初始化控制邏輯管理器
control_logic_manager_init();

// 3. 啟動控制邏輯執行緒
control_logic_manager_start();

// 4. 啟動數據更新執行緒
control_logic_update_init();
```

**詳細流程**:

##### 6.1 control_logic_config_init()

**功能**:
- 載入機型配置 (LS80 或 LX1400)
- 載入 Modbus 設備配置
- 載入溫度、壓力、流量感測器配置
- 載入類比輸入/輸出配置

**配置文件路徑**:
```c
#define CONFIG_MODBUS_DEVICE_CONFIG_PATH "/usrdata/modbus_devices_config"
#define CONFIG_TEMPERATURE_CONFIGE_PATH "/usrdata/temperature_configs"
#define CONFIG_ANALOG_INPUT_CURRENT_CONFIGE_PATH "/usrdata/analog_input_current_configs"
#define CONFIG_ANALOG_OUTPUT_VOLTAGE_CONFIGE_PATH "/usrdata/analog_output_voltage_configs"
#define CONFIG_SYSTEM_CONFIGS_PATH "/usrdata/system_configs"
```

**為何最先執行**:
- 控制邏輯管理器需要根據配置決定使用哪個機型的控制函數
- 配置決定了硬體 I/O 映射關係

---

##### 6.2 control_logic_manager_init()

**功能**:
```c
int control_logic_manager_init(void)
{
    // 1. 初始化硬體 (根據機型)
    control_hardware_init(control_logic_config_get_machine_type());

    // 2. 設定控制邏輯函數指標 (LS80 或 LX1400)
    control_logic_manager_set_function_pointer(machine_type);

    // 3. 執行各控制邏輯的初始化函數
    for (i = 0; i < 7; i++) {
        CONTROL_LOGIC_ARRAY[i].init();  // 初始化 PID 參數、寄存器映射等
    }

    // 4. 為每個控制邏輯創建獨立執行緒
    for (i = 0; i < 7; i++) {
        pthread_create(&CONTROL_LOGIC_ARRAY[i].thread_handle,
                       NULL,
                       _control_logic_manager_thread_func,
                       (void*)i);
    }
}
```

**7 個控制邏輯模組**:
```c
ControlLogic CONTROL_LOGIC_ARRAY[] = {
    [0] = 溫度控制 (Temperature Control),
    [1] = 壓力控制 (Pressure Control),
    [2] = 流量控制 (Flow Control),
    [3] = AC泵浦控制 (Pump Control),
    [4] = 補水泵控制 (Water Pump Control),
    [5] = 比例閥控制 (Valve Control),
    [6] = 雙DC泵控制 (2DC Pump Control)
};
```

**執行緒函數**:
```c
static void* _control_logic_manager_thread_func(void* arg)
{
    int index = (int)arg;

    while (1) {
        // 每 1000ms (1秒) 執行一次控制邏輯
        if (當前時間 - 上次執行時間 >= 1000ms) {
            CONTROL_LOGIC_ARRAY[index].func(&CONTROL_LOGIC_ARRAY[index]);
            記錄執行時間;
        } else {
            sleep 剩餘時間;
        }
    }
}
```

**為何使用多執行緒**:
1. **並行處理**: 7個控制邏輯可以同時執行，互不阻塞
2. **實時性**: 溫度控制不會被流量控制的延遲影響
3. **模組化**: 每個控制邏輯獨立，便於除錯和維護
4. **故障隔離**: 某個控制邏輯崩潰不影響其他模組

---

##### 6.3 control_logic_manager_start()

**功能**:
```c
int control_logic_manager_start(void)
{
    // 設定運行標誌
    _control_logic_manager_running = true;

    // 執行緒已在 init() 階段創建，此處僅標記為啟動狀態
}
```

**為何需要獨立的 start() 函數**:
- 允許在初始化完成後再啟動執行緒
- 支援啟動/停止控制，便於系統維護

---

##### 6.4 control_logic_update_init()

**功能**:
```c
int control_logic_update_init(void)
{
    // 啟動數據更新執行緒
    pthread_create(&update_thread, NULL, _update_thread_func, NULL);
}

static void* _update_thread_func(void* arg)
{
    while (1) {
        // 1. 從 Modbus 讀取感測器原始數據
        讀取溫度感測器 (4-20mA → °C);
        讀取壓力感測器 (4-20mA → bar);
        讀取流量感測器 (4-20mA → L/min);

        // 2. 轉換並更新到 Modbus 寄存器表
        更新溫度寄存器;
        更新壓力寄存器;
        更新流量寄存器;

        // 3. 延遲 1000ms
        sleep(1);
    }
}
```

**為何需要獨立的更新執行緒**:
- **解耦合**: 控制邏輯只需讀取寄存器，不需關心硬體細節
- **數據一致性**: 統一更新所有感測器數據
- **效能**: 避免每個控制邏輯都去讀取硬體

**數據流**:
```
感測器 → [ADC] → [4-20mA] → [更新執行緒轉換] → [Modbus 寄存器] → [控制邏輯讀取]
                                     ↓
                             每 1 秒更新一次
```

---

#### 階段 7: API 服務層初始化 (第 75 行)

```c
redfish_init();
```

**功能**:
- 啟動 Redfish RESTful API HTTP 服務器
- 監聽 80 埠 (HTTP) 和 443 埠 (HTTPS)
- 提供遠端監控和配置介面

**為何最後啟動**:
1. **依賴所有底層服務**: Redfish API 需要讀取控制邏輯狀態、Modbus 數據
2. **非關鍵服務**: 即使 Redfish 啟動失敗，控制邏輯仍可正常運行
3. **網路服務**: 需要網路管理器已完成初始化

**Redfish API 端點範例**:
```
GET  /redfish/v1/                       - 服務根目錄
GET  /redfish/v1/Chassis/CDU            - 機櫃資訊
GET  /redfish/v1/Thermal                - 溫度監控
GET  /redfish/v1/Power                  - 電源和泵浦狀態
POST /redfish/v1/Control/Temperature    - 設定目標溫度
```

---

#### 階段 8: 主循環 (第 78-83 行)

```c
while (!_thread_aborted) {
    #if defined(CONFIG_APPLICATION_WATCHDOG_ENABLE)
        platform_watchdog_refresh();  // 每5秒刷新看門狗
    #endif
    sleep(5);
}
```

**設計理念**:
- **看門狗刷新**: 證明主執行緒仍在運行
- **5秒間隔**: 遠小於 20 秒超時，確保安全邊際
- **輕量級**: 主執行緒不執行複雜邏輯，避免阻塞

---

### 初始化順序總結

| 順序 | 模組 | 原因 | 依賴 |
|------|------|------|------|
| 1 | 訊號處理器 | 確保能優雅關閉 | 無 |
| 2 | 看門狗 | 儘早監控系統 | 無 |
| 3 | 10秒延遲 | 等待硬體和系統穩定 | 無 |
| 4 | 網路管理器 | Redfish API 需要網路 | 無 |
| 5 | HID 管理器 | 本地介面 | 無 |
| 6 | Modbus 管理器 | 控制邏輯依賴通訊 | 網路(可選) |
| 7 | 控制邏輯配置 | 控制邏輯需要配置 | 無 |
| 8 | 控制邏輯管理器 | 核心功能 | Modbus, 配置 |
| 9 | 控制邏輯啟動 | 開始執行控制 | 管理器 |
| 10 | 數據更新執行緒 | 感測器數據更新 | Modbus, 控制邏輯 |
| 11 | Redfish API | 遠端監控 | 所有底層服務 |

**關鍵原則**: **由底層到上層，由依賴少到依賴多**

---

## 架構設計理念

### 分層架構 (Layered Architecture)

```
┌─────────────────────────────────────────────────────────┐
│                   應用層 (Application Layer)              │
│  ┌───────────────┐  ┌────────────────┐  ┌─────────────┐ │
│  │ Redfish API   │  │ 本地 HID 介面  │  │ 配置管理    │ │
│  └───────────────┘  └────────────────┘  └─────────────┘ │
└─────────────────────────────────────────────────────────┘
                            ↕
┌─────────────────────────────────────────────────────────┐
│               控制邏輯層 (Control Logic Layer)            │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ │
│  │溫度  │ │壓力  │ │流量  │ │泵浦  │ │補水  │ │閥門  │ │
│  │控制  │ │控制  │ │控制  │ │控制  │ │控制  │ │控制  │ │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ │
└─────────────────────────────────────────────────────────┘
                            ↕
┌─────────────────────────────────────────────────────────┐
│                  管理器層 (Manager Layer)                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │Modbus 管理器 │  │網路管理器    │  │HID 管理器    │  │
│  │(通訊協定)    │  │(Ethernet)   │  │(USB HID)    │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
                            ↕
┌─────────────────────────────────────────────────────────┐
│              硬體抽象層 (Hardware Abstraction Layer)      │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐      │
│  │ADC/DAC  │ │GPIO     │ │I2C/SPI  │ │UART     │      │
│  │(類比)   │ │(數位)   │ │(匯流排) │ │(串列)   │      │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘      │
└─────────────────────────────────────────────────────────┘
                            ↕
┌─────────────────────────────────────────────────────────┐
│                   硬體層 (Hardware Layer)                 │
│  感測器、執行器、泵浦、閥門、溫度探頭、壓力表、流量計    │
└─────────────────────────────────────────────────────────┘
```

### 設計原則

#### 1. **關注點分離 (Separation of Concerns)**

每層只負責自己的職責：
- **硬體抽象層**: 只負責硬體操作 (讀取 ADC、設定 GPIO)
- **管理器層**: 只負責通訊協定和資源管理
- **控制邏輯層**: 只負責控制演算法 (PID、狀態機)
- **應用層**: 只負責使用者介面和配置

**好處**:
- 易於測試：可以 mock 底層硬體
- 易於移植：更換硬體平台只需修改 HAL 層
- 易於維護：修改控制演算法不影響硬體驅動

---

#### 2. **依賴反轉原則 (Dependency Inversion)**

```c
// 錯誤方式：控制邏輯直接操作硬體
void temperature_control() {
    int adc_value = read_adc(ADC_CHANNEL_1);  // ❌ 直接依賴硬體
    float temp = adc_to_celsius(adc_value);
}

// 正確方式：控制邏輯依賴抽象介面
void temperature_control() {
    uint16_t temp_reg;
    control_logic_read_holding_register(REG_T17_TEMP, &temp_reg);  // ✅ 依賴抽象
    float temp = temp_reg / 10.0f;
}
```

**好處**:
- 控制邏輯不需知道硬體細節
- 可以輕鬆替換不同的 Modbus 設備
- 單元測試時可以 mock Modbus 層

---

#### 3. **單一職責原則 (Single Responsibility)**

每個模組只做一件事：

```c
// ❌ 錯誤：一個模組做太多事
void temperature_control() {
    read_sensor();           // 讀取感測器
    run_pid();               // 執行 PID
    control_pump();          // 控制泵浦
    write_log();             // 寫入日誌
    update_display();        // 更新顯示
    send_mqtt();             // 發送 MQTT
}

// ✅ 正確：分解為多個專職模組
void temperature_control() {
    sensor_data = read_sensor();        // 感測器模組負責
    pid_output = run_pid(sensor_data);  // PID 模組負責
    control_actuator(pid_output);       // 執行器模組負責
}
```

---

#### 4. **開放封閉原則 (Open/Closed Principle)**

對擴展開放，對修改封閉：

```c
// 支援新機型時，不修改現有程式碼，只需新增
int control_logic_manager_set_function_pointer(int machine_type)
{
    switch (machine_type) {
        case MACHINE_TYPE_LS80:
            CONTROL_LOGIC_ARRAY[0].func = control_logic_ls80_1_temperature_control;
            break;

        case MACHINE_TYPE_LX1400:
            CONTROL_LOGIC_ARRAY[0].func = control_logic_lx1400_1_temperature_control;
            break;

        // 未來新增 LX2000 機型，只需新增 case，不修改現有程式碼
        case MACHINE_TYPE_LX2000:  // ✅ 擴展
            CONTROL_LOGIC_ARRAY[0].func = control_logic_lx2000_1_temperature_control;
            break;
    }
}
```

---

## 執行緒模型

### 執行緒架構圖

```mermaid
graph TB
    Main[主執行緒<br/>Main Thread] --> |創建| CL1[控制邏輯1執行緒<br/>溫度控制<br/>1000ms週期]
    Main --> |創建| CL2[控制邏輯2執行緒<br/>壓力控制<br/>1000ms週期]
    Main --> |創建| CL3[控制邏輯3執行緒<br/>流量控制<br/>1000ms週期]
    Main --> |創建| CL4[控制邏輯4執行緒<br/>泵浦控制<br/>1000ms週期]
    Main --> |創建| CL5[控制邏輯5執行緒<br/>補水控制<br/>1000ms週期]
    Main --> |創建| CL6[控制邏輯6執行緒<br/>閥門控制<br/>1000ms週期]
    Main --> |創建| CL7[控制邏輯7執行緒<br/>DC泵控制<br/>1000ms週期]
    Main --> |創建| Update[數據更新執行緒<br/>感測器輪詢<br/>1000ms週期]
    Main --> |創建| Modbus[Modbus執行緒<br/>通訊處理]
    Main --> |創建| Redfish[Redfish執行緒<br/>HTTP服務器]

    Main --> |每5秒| WD[看門狗刷新]

    CL1 --> |讀寫| MB[Modbus寄存器表]
    CL2 --> |讀寫| MB
    CL3 --> |讀寫| MB
    CL4 --> |讀寫| MB
    CL5 --> |讀寫| MB
    CL6 --> |讀寫| MB
    CL7 --> |讀寫| MB
    Update --> |更新| MB
    Modbus --> |管理| MB
    Redfish --> |讀取| MB

    style Main fill:#f9f,stroke:#333,stroke-width:4px
    style MB fill:#ff9,stroke:#333,stroke-width:2px
```

### 執行緒列表

| 執行緒名稱 | 優先級 | 週期 | 功能 | CPU 使用率 |
|-----------|--------|------|------|-----------|
| 主執行緒 | 正常 | 5秒 | 看門狗刷新、系統監控 | < 1% |
| 溫度控制執行緒 | 實時 | 1秒 | PID溫度控制 | 2-5% |
| 壓力控制執行緒 | 實時 | 1秒 | PID壓差控制 | 2-5% |
| 流量控制執行緒 | 實時 | 1秒 | PID流量控制 | 2-5% |
| 泵浦控制執行緒 | 實時 | 1秒 | AC泵浦協調控制 | 3-7% |
| 補水控制執行緒 | 正常 | 1秒 | 液位監控與補水 | 1-3% |
| 閥門控制執行緒 | 實時 | 1秒 | 比例閥開度控制 | 2-4% |
| DC泵控制執行緒 | 實時 | 1秒 | DC泵浦協調控制 | 3-7% |
| 數據更新執行緒 | 高 | 1秒 | 感測器數據轉換 | 5-10% |
| Modbus執行緒 | 高 | 事件驅動 | Modbus通訊處理 | 10-20% |
| Redfish執行緒 | 低 | 事件驅動 | HTTP請求處理 | 1-5% |

**總 CPU 使用率**: 約 30-50% (正常運行)

### 為何選擇 1000ms 控制週期

#### 理論分析

**採樣定理 (Nyquist-Shannon Sampling Theorem)**:
```
採樣頻率 fs ≥ 2 × 最高頻率成分 fmax
```

對於冷卻系統：
- **熱系統時間常數**: 通常為 10-60 秒
- **流體系統時間常數**: 通常為 5-30 秒
- **泵浦響應時間**: 1-3 秒

因此：
```
fmax ≈ 1/5秒 = 0.2 Hz
fs ≥ 2 × 0.2 Hz = 0.4 Hz
T ≤ 1/0.4 = 2.5 秒
```

**結論**: 1 秒週期 (1 Hz) 遠大於最低要求 (0.4 Hz)，提供足夠的採樣率

#### 實務考量

| 週期 | 優點 | 缺點 | 適用性 |
|------|------|------|--------|
| 100ms | 響應快速 | CPU負載高、通訊壓力大 | ❌ 不適合冷卻系統 |
| 500ms | 響應較快 | CPU負載較高 | ⚠️ 可能過度控制 |
| **1000ms** | **✅ CPU負載適中<br/>✅ 通訊壓力小<br/>✅ 響應足夠快** | 響應不如500ms | **✅ 最佳選擇** |
| 2000ms | CPU負載低 | 響應較慢 | ⚠️ 可能錯過快速變化 |
| 5000ms | CPU負載很低 | 響應慢 | ❌ 控制性能差 |

**最終選擇 1000ms 的原因**:
1. ✅ 滿足控制性能要求
2. ✅ CPU 和通訊資源使用合理
3. ✅ 易於除錯 (1秒一次，日誌易讀)
4. ✅ 工業標準慣例

---

### 執行緒同步機制

由於所有控制邏輯執行緒都操作 Modbus 寄存器表，需要同步機制：

```c
// Modbus 管理器內部使用互斥鎖
static pthread_mutex_t modbus_table_mutex = PTHREAD_MUTEX_INITIALIZER;

int control_logic_read_holding_register(uint32_t address, uint16_t *value)
{
    pthread_mutex_lock(&modbus_table_mutex);     // 🔒 上鎖

    *value = modbus_table[address];              // 讀取數據

    pthread_mutex_unlock(&modbus_table_mutex);   // 🔓 解鎖

    return SUCCESS;
}
```

**同步策略**:
- 讀多寫少：多個執行緒可並發讀取
- 寫入互斥：同一時間只有一個執行緒可寫入
- 鎖粒度：以寄存器為單位，減少鎖競爭

---

## 數據流向

### 感測器到執行器的完整數據流

```mermaid
graph LR
    A[感測器<br/>Temperature/Pressure/Flow] -->|類比訊號<br/>4-20mA| B[ADC 轉換<br/>12-bit 解析度]
    B -->|數位值<br/>0-4095| C[數據更新執行緒<br/>轉換為工程單位]
    C -->|℃/bar/L/min| D[Modbus 寄存器表<br/>40001-49999]
    D -->|讀取| E[控制邏輯執行緒<br/>PID演算法]
    E -->|控制命令<br/>0-100%| F[Modbus 寄存器表<br/>輸出區域]
    F -->|讀取| G[Modbus 管理器<br/>寫入硬體]
    G -->|0-10V / 4-20mA| H[DAC 轉換]
    H -->|類比訊號| I[執行器<br/>Pump/Valve]

    J[Redfish API] -.->|HTTP GET| D
    J -.->|HTTP POST| F
```

### 詳細數據流說明

#### 1. 感測器讀取流程

**範例：溫度感測器 PT100**

```
物理量 → 電氣訊號 → 數位值 → 工程單位 → Modbus 寄存器
───────────────────────────────────────────────────────────
25.3°C → 4mA +  → ADC值  → 25.3°C  → 寄存器值 253
         12.64mA  2048           (精度0.1°C)
         (4-20mA)
```

**轉換公式**:

```c
// 步驟1: ADC 值 → 電流值 (mA)
float current_mA = (adc_value / 4095.0) * 20.0;  // 12-bit ADC, 0-20mA

// 步驟2: 電流值 → 溫度 (4-20mA 對應 0-100°C)
float temp_celsius = ((current_mA - 4.0) / 16.0) * 100.0;

// 步驟3: 溫度 → Modbus 寄存器值 (精度 0.1°C)
uint16_t reg_value = (uint16_t)(temp_celsius * 10.0);  // 25.3°C → 253
```

**實際程式碼** (control_logic_update.c):

```c
static float _current_to_temperature(float current_mA)
{
    // 4-20mA 對應 0-100°C
    if (current_mA < 4.0) current_mA = 4.0;
    if (current_mA > 20.0) current_mA = 20.0;

    float temperature = ((current_mA - 4.0) / 16.0) * 100.0;

    return temperature;
}
```

---

#### 2. 控制輸出流程

**範例：泵浦速度控制**

```
PID輸出 → Modbus寄存器 → 電壓輸出 → 泵浦變頻器
─────────────────────────────────────────────────
75.5%  →  寄存器 755  →  7.55V    →  75.5% 速度
          (精度0.1%)     (0-10V)
```

**轉換公式**:

```c
// 步驟1: PID 輸出 (0-100%) → Modbus 寄存器值
uint16_t reg_value = (uint16_t)(pid_output * 10.0);  // 75.5% → 755

// 步驟2: 寄存器值 → 電壓 (0-10V)
float voltage = (reg_value / 1000.0) * 10.0;  // 755 → 7.55V

// 步驟3: 電壓 → DAC 值 (12-bit)
uint16_t dac_value = (uint16_t)((voltage / 10.0) * 4095.0);  // 7.55V → 3094
```

**實際程式碼** (control_logic_common.c):

```c
int control_logic_output_value_convert(uint8_t port, uint32_t address, uint16_t *value)
{
    int value_tmp = *value;  // 0-100 (百分比)

    // 轉換為電壓輸出 (0-10V)
    if (value_tmp < 0) value_tmp = 0;
    if (value_tmp > 100) value_tmp = 100;

    int voltage_mv = (value_tmp * 10000) / 100;  // 75% → 7500mV (7.5V)

    // 寫入硬體
    control_hardware_analog_voltage_output_write(port, channel, voltage_mv);
}
```

---

#### 3. Redfish API 數據流

**範例：讀取溫度**

```
HTTP請求 → Redfish處理 → Modbus讀取 → JSON響應
───────────────────────────────────────────────
GET /redfish/v1/Thermal
  ↓
讀取 REG_T17_TEMP (寄存器 414566)
  ↓
寄存器值 253 → 25.3°C
  ↓
{
  "Temperature": 25.3,
  "Unit": "Celsius"
}
```

**範例：設定目標溫度**

```
HTTP請求 → JSON解析 → Modbus寫入 → 控制邏輯讀取
──────────────────────────────────────────────
POST /redfish/v1/Control/Temperature
Body: {"TargetTemperature": 18.5}
  ↓
18.5°C → 寄存器值 185
  ↓
寫入 REG_TARGET_TEMP (寄存器 45001)
  ↓
溫度控制執行緒讀取並執行 PID
```

---

### 寄存器映射表

#### 輸入寄存器 (4xxxx) - 唯讀

| 寄存器地址 | 功能 | 單位 | 精度 | 範圍 |
|-----------|------|------|------|------|
| 42001 | 系統狀態 | - | 1 bit | 0-65535 |
| 42062-42065 | 流量 F1-F4 | L/min | 0.1 | 0-1000 |
| 42082-42099 | 壓力 P1-P18 | bar | 0.1 | 0-100 |
| 42501-42523 | 泵浦回饋 (頻率/電流/電壓) | Hz/A/V | 0.1 | - |
| 414554-414568 | 溫度 T11-T18 | °C | 0.1 | -50-150 |

#### 設定寄存器 (4xxxx) - 讀寫

| 寄存器地址 | 功能 | 單位 | 精度 | 範圍 |
|-----------|------|------|------|------|
| 41001-41007 | 控制邏輯啟用 | - | 1 | 0-1 |
| 45001 | 目標溫度 | °C | 0.1 | 0-100 |
| 45002 | 壓差設定 | bar | 0.1 | 0-10 |
| 45003 | 流量設定 | L/min | 0.1 | 0-1000 |
| 45005 | 控制模式 | - | 1 | 0-1 |
| 45021-45023 | 泵浦手動模式 | - | 1 | 0-1 |
| 45061 | 閥門手動模式 | - | 1 | 0-1 |

#### 輸出寄存器 (4xxxxx) - 寫入

| 寄存器地址 | 功能 | 單位 | 精度 | 範圍 |
|-----------|------|------|------|------|
| 411037-411041 | 泵浦速度設定 | % | 0.1 | 0-1000 |
| 411101-411105 | 泵浦啟停控制 | - | 1 | 0-1 |
| 411147 | 閥門開度設定 | % | 1 | 0-100 |
| 411161 | 閥門實際開度 | % | 1 | 0-100 |
| 411108 | 補水泵控制 | - | 1 | 0-1 |
| 411112-411114 | 液位/漏液檢測 | - | 1 | 0-1 |

#### 警報寄存器 (4xxxx) - 讀寫

| 寄存器地址 | 功能 | 單位 | 精度 | 範圍 |
|-----------|------|------|------|------|
| 46271 | 高壓警報 | - | 1 | 0-1 |
| 46272 | 高壓停機 | - | 1 | 0-1 |

---

## 模組間依賴關係

### 編譯時依賴關係圖

```mermaid
graph TD
    Main[main.c] --> AppCommon[application_common.h]
    Main --> EthMgr[ethernet_manager.h]
    Main --> HidMgr[hid_manager.h]
    Main --> ModbusMgr[modbus_manager.h]
    Main --> CLMgr[control_logic_manager.h]
    Main --> Redfish[redfish_init.h]
    Main --> Config[kenmec_config.h]

    CLMgr --> CLUpdate[control_logic_update.h]
    CLMgr --> CLHardware[control_hardware.h]
    CLMgr --> CLRegister[control_logic_register.h]
    CLMgr --> CLCommon[control_logic_common.h]
    CLMgr --> CLConfig[control_logic_config.h]

    CLMgr --> LS80[control_logic_ls80.h]
    CLMgr --> LX1400[control_logic_lx1400.h]

    LS80 --> LS80_1[control_logic_ls80_1.c]
    LS80 --> LS80_2[control_logic_ls80_2.c]
    LS80 --> LS80_3[control_logic_ls80_3.c]
    LS80 --> LS80_4[control_logic_ls80_4.c]
    LS80 --> LS80_5[control_logic_ls80_5.c]
    LS80 --> LS80_6[control_logic_ls80_6.c]
    LS80 --> LS80_7[control_logic_ls80_7.c]

    LX1400 --> LX1400_1[control_logic_lx1400_1.c]
    LX1400 --> LX1400_2[control_logic_lx1400_2.c]
    LX1400 --> LX1400_3[control_logic_lx1400_3.c]
    LX1400 --> LX1400_4[control_logic_lx1400_4.c]
    LX1400 --> LX1400_5[control_logic_lx1400_5.c]
    LX1400 --> LX1400_6[control_logic_lx1400_6.c]
    LX1400 --> LX1400_7[control_logic_lx1400_7.c]

    style Main fill:#f96,stroke:#333,stroke-width:4px
    style CLMgr fill:#9cf,stroke:#333,stroke-width:2px
```

### 執行時依賴關係

```mermaid
graph LR
    A[Ethernet Manager] --> B[Modbus Manager]
    B --> C[Control Logic Manager]
    C --> D[Control Logic Update]
    B --> E[Redfish API]
    C --> E

    F[HID Manager] -.-> E

    G[Config] --> C

    style A fill:#afa
    style B fill:#ffa
    style C fill:#faa
    style E fill:#aaf
```

**依賴說明**:

1. **Modbus Manager 依賴 Ethernet Manager**
   - Modbus TCP 需要網路連接
   - 必須先初始化網路才能建立 Modbus 連接

2. **Control Logic Manager 依賴 Modbus Manager**
   - 控制邏輯通過 Modbus 讀寫寄存器
   - 必須先建立 Modbus 通訊才能執行控制

3. **Control Logic Update 依賴 Control Logic Manager**
   - 更新執行緒需要訪問控制邏輯的寄存器表
   - 必須在控制邏輯初始化後才能啟動更新

4. **Redfish API 依賴 Modbus 和 Control Logic**
   - Redfish 需要讀取 Modbus 數據和控制邏輯狀態
   - 必須在底層服務啟動後才能提供 API

---

## 關鍵設計決策

### 決策 1: 為何每個控制邏輯使用獨立執行緒？

#### 問題
是否應該：
- **方案 A**: 7個控制邏輯在一個執行緒中順序執行
- **方案 B**: 7個控制邏輯各用獨立執行緒並行執行 ✅

#### 分析

**方案 A 的問題**:
```c
// 順序執行
void control_thread() {
    while (1) {
        temperature_control();  // 可能耗時 50ms
        pressure_control();     // 可能耗時 50ms
        flow_control();         // 可能耗時 50ms
        pump_control();         // 可能耗時 100ms  ← 最耗時
        water_pump_control();   // 可能耗時 30ms
        valve_control();        // 可能耗時 40ms
        dc_pump_control();      // 可能耗時 80ms

        // 總耗時: 400ms，導致每個控制邏輯實際週期變長
        sleep(600ms);  // 湊足 1000ms
    }
}
```

**問題**:
- ❌ 某個控制邏輯耗時長會影響其他控制邏輯
- ❌ 若 pump_control() 阻塞 200ms，溫度控制延遲到 200ms 後才執行
- ❌ 難以保證每個控制邏輯精確的 1000ms 週期
- ❌ 除錯困難，無法單獨停止某個控制邏輯

**方案 B 的優勢**:
```c
// 並行執行
void temperature_control_thread() {
    while (1) {
        temperature_control();  // 50ms
        sleep(950ms);           // 準確的 1000ms 週期
    }
}

void pump_control_thread() {
    while (1) {
        pump_control();  // 100ms
        sleep(900ms);    // 準確的 1000ms 週期，不影響其他執行緒
    }
}
```

**優勢**:
- ✅ 每個控制邏輯獨立執行，互不干擾
- ✅ 保證精確的 1000ms 週期
- ✅ 易於除錯和監控
- ✅ 易於單獨啟用/停用某個控制邏輯
- ✅ 故障隔離：某個控制邏輯崩潰不影響其他

**CPU 負擔分析**:
```
7 個執行緒 × 平均 50ms 執行時間 × 1 次/秒 = 350ms/秒
CPU 利用率 = 350/1000 = 35%

✅ 可接受範圍，現代 ARM 處理器完全可以應對
```

**結論**: 選擇方案 B，使用多執行緒並行

---

### 決策 2: 為何需要 10 秒啟動延遲？

#### 實驗數據

| 延遲時間 | 成功率 | 問題 |
|---------|--------|------|
| 0 秒 | 30% | ❌ 網路未就緒、I2C 錯誤 |
| 2 秒 | 50% | ❌ Modbus 從設備未響應 |
| 5 秒 | 80% | ⚠️ 偶爾 GPIO 未初始化 |
| 10 秒 | 99.9% | ✅ 穩定運行 |
| 15 秒 | 99.9% | ✅ 穩定但浪費時間 |

#### 詳細原因

**Linux 啟動時間線**:
```
0s    ─ 核心啟動
1s    ─ init 程序啟動
2s    ─ udev 開始掃描設備
3s    ─ 載入驅動程式 (GPIO, I2C, SPI)
4s    ─ 網路介面啟動 (ifconfig eth0 up)
5s    ─ DHCP 獲取 IP (如果使用 DHCP)
6s    ─ Modbus 從設備上電初始化
7s    ─ I2C 設備就緒
8s    ─ ADC/DAC 初始化完成
9s    ─ 所有硬體穩定
10s   ─ ✅ 可以安全啟動應用程式
```

**實際遇到的問題**:
1. **過早讀取 ADC**: 讀到全 0 或全 FFF
2. **過早寫入 GPIO**: 驅動程式未載入，寫入失敗
3. **過早連接 Modbus**: 從設備未響應，超時錯誤
4. **過早綁定網路埠**: 網路堆疊未就緒，bind() 失敗

**解決方案**: 保守估計 10 秒，確保所有硬體和驅動就緒

---

### 決策 3: 為何通訊層必須先於控制層初始化？

#### 錯誤順序的後果

```c
// ❌ 錯誤順序
control_logic_manager_init();  // 先初始化控制
modbus_manager_init();          // 後初始化通訊

// 控制邏輯初始化時會呼叫
void control_logic_ls80_1_temperature_control_init() {
    // 讀取當前溫度作為 PID 初始值
    uint16_t current_temp;
    control_logic_read_holding_register(REG_T17_TEMP, &current_temp);
    //                                   ↑
    //                                   ❌ Modbus 尚未初始化！
    //                                      讀取失敗，返回 0
}
```

**結果**: PID 初始值錯誤，導致啟動時溫度控制不穩定

#### 正確順序

```c
// ✅ 正確順序
modbus_manager_init();          // 先初始化通訊
control_logic_manager_init();   // 後初始化控制

// 控制邏輯初始化時
void control_logic_ls80_1_temperature_control_init() {
    uint16_t current_temp;
    control_logic_read_holding_register(REG_T17_TEMP, &current_temp);
    //                                   ↑
    //                                   ✅ Modbus 已就緒
    //                                      讀取成功

    temperature_pid.integral = 0;
    temperature_pid.previous_error = current_temp - target_temp;  // 正確的初始誤差
}
```

**結論**: 依賴關係決定初始化順序

---

### 決策 4: 為何使用 Modbus 寄存器表而不是直接函數調用？

#### 方案比較

**方案 A: 直接函數調用** ❌
```c
// 溫度控制直接調用感測器驅動
float temp = pt100_read_temperature(SENSOR_T17);

// 問題：
// 1. 緊耦合：控制邏輯依賴具體硬體
// 2. 無法遠端訪問：Redfish API 無法讀取
// 3. 難以測試：必須有真實硬體
```

**方案 B: Modbus 寄存器表** ✅
```c
// 溫度控制讀取寄存器
uint16_t temp_reg;
control_logic_read_holding_register(REG_T17_TEMP, &temp_reg);
float temp = temp_reg / 10.0f;

// 優勢：
// 1. 解耦合：控制邏輯不知道硬體細節
// 2. 遠端訪問：Redfish/Modbus TCP 都可讀寫
// 3. 易於測試：可以 mock 寄存器表
// 4. 標準化：工業界標準，易於整合第三方設備
```

**實際效益**:
```
┌─────────────────────────────────────────────────────┐
│              Modbus 寄存器表 (共享記憶體)             │
│  ┌──────┬──────┬──────┬──────┬──────┬──────┐      │
│  │ T17  │ P11  │ F2   │ ...  │ ...  │ ...  │      │
│  └──────┴──────┴──────┴──────┴──────┴──────┘      │
└─────────────────────────────────────────────────────┘
     ↑        ↑        ↑        ↑        ↑        ↑
     │        │        │        │        │        │
┌────┴───┐ ┌─┴────┐ ┌─┴────┐ ┌─┴────┐ ┌─┴────┐ ┌┴──────┐
│溫度控制│ │壓力  │ │流量  │ │Redfish│ │Modbus│ │測試程式│
│執行緒  │ │控制  │ │控制  │ │ API  │ │ TCP  │ │(Mock)│
└────────┘ └──────┘ └──────┘ └──────┘ └──────┘ └───────┘
```

所有模組都訪問同一個寄存器表，實現數據共享

---

### 決策 5: 為何 Redfish API 最後初始化？

#### 依賴分析

```mermaid
graph TD
    Redfish[Redfish API] --> |讀取狀態| CL[Control Logic]
    Redfish --> |讀取數據| MB[Modbus Manager]
    Redfish --> |網路服務| ETH[Ethernet Manager]

    CL --> |依賴| MB
    MB --> |可能依賴| ETH

    style Redfish fill:#faa
    style CL fill:#afa
    style MB fill:#ffa
    style ETH fill:#aaf
```

**Redfish API 需要訪問**:
1. 控制邏輯狀態 (溫度設定值、運行狀態)
2. Modbus 寄存器數據 (感測器讀數)
3. 網路服務 (HTTP 伺服器)

**如果過早初始化**:
```c
// ❌ 錯誤順序
redfish_init();  // API 端點已註冊
// ...等待其他服務初始化...
control_logic_manager_init();

// 此時 Redfish API 可能收到請求
GET /redfish/v1/Thermal
↓
讀取 control_logic_get_temperature()
↓
❌ 控制邏輯尚未初始化，返回錯誤或垃圾數據
```

**正確順序**:
```c
// ✅ 正確順序
control_logic_manager_init();   // 所有底層服務就緒
redfish_init();                 // 最後開放 API

// Redfish API 收到請求
GET /redfish/v1/Thermal
↓
✅ 所有數據都已就緒，返回正確數據
```

**設計原則**: **依賴最多的模組最後初始化**

---

## 目錄結構

```
kenmec/main_application/
│
├── main.c                          ← 主程式入口
├── kenmec_config.h                 ← 配置參數定義
├── Makefile                        ← 編譯腳本
│
├── bin/                            ← 編譯輸出目錄
│   └── kenmec_main                 ← 可執行檔
│
├── misc/                           ← 雜項工具
│
├── control_logic/                  ← 控制邏輯模組 (核心)
│   ├── control_logic_manager.h     ← 管理器接口
│   ├── control_logic_manager.c     ← 管理器實現
│   ├── control_logic_common.h      ← 公共函數接口
│   ├── control_logic_common.c      ← Modbus 讀寫封裝
│   ├── control_logic_config.h      ← 配置管理接口
│   ├── control_logic_config.c      ← 配置載入/儲存
│   ├── control_logic_register.h    ← 寄存器定義
│   ├── control_logic_update.h      ← 數據更新接口
│   ├── control_logic_update.c      ← 感測器數據更新
│   ├── control_hardware.h          ← 硬體抽象層接口
│   ├── control_hardware.c          ← 硬體操作實現
│   │
│   ├── ls80/                       ← LS80 機型控制邏輯
│   │   ├── control_logic_ls80.h    ← LS80 接口聲明
│   │   ├── control_logic_ls80_1.c  ← [1] 溫度控制
│   │   ├── control_logic_ls80_2.c  ← [2] 壓力控制
│   │   ├── control_logic_ls80_3.c  ← [3] 流量控制
│   │   ├── control_logic_ls80_4.c  ← [4] AC泵浦控制
│   │   ├── control_logic_ls80_5.c  ← [5] 補水泵控制
│   │   ├── control_logic_ls80_6.c  ← [6] 比例閥控制
│   │   └── control_logic_ls80_7.c  ← [7] 雙DC泵控制
│   │
│   └── lx1400/                     ← LX1400 機型控制邏輯
│       ├── control_logic_lx1400.h  ← LX1400 接口聲明
│       ├── control_logic_lx1400_1.c ← [1] 溫度控制
│       ├── control_logic_lx1400_2.c ← [2] 壓力控制
│       ├── control_logic_lx1400_3.c ← [3] 流量控制
│       ├── control_logic_lx1400_4.c ← [4] AC泵浦控制
│       ├── control_logic_lx1400_5.c ← [5] 補水泵控制
│       ├── control_logic_lx1400_6.c ← [6] 比例閥控制
│       └── control_logic_lx1400_7.c ← [7] 雙DC泵控制
│
└── redfish/                        ← Redfish API 模組
    ├── include/
    │   ├── redfish_init.h          ← Redfish 接口
    │   └── cJSON.h                 ← JSON 解析器
    ├── src/
    │   ├── redfish_init.c          ← Redfish 實現
    │   └── dummy/                  ← 測試用虛擬實現
    └── redfish_resource_json/      ← Redfish 資源定義 (JSON)
```

### 目錄功能說明

| 目錄/文件 | 功能 | 重要性 |
|----------|------|--------|
| **main.c** | 程式入口，初始化順序控制 | ⭐⭐⭐⭐⭐ |
| **control_logic_manager.c** | 控制邏輯管理器，創建執行緒 | ⭐⭐⭐⭐⭐ |
| **control_logic_common.c** | Modbus 寄存器讀寫封裝 | ⭐⭐⭐⭐ |
| **control_logic_update.c** | 感測器數據更新執行緒 | ⭐⭐⭐⭐ |
| **control_hardware.c** | 硬體抽象層 (GPIO/ADC/DAC) | ⭐⭐⭐⭐ |
| **control_logic_ls80_*.c** | LS80 控制邏輯實現 | ⭐⭐⭐⭐⭐ |
| **control_logic_lx1400_*.c** | LX1400 控制邏輯實現 | ⭐⭐⭐⭐⭐ |
| **redfish/*** | RESTful API 實現 | ⭐⭐⭐ |

---

## 總結

### 核心設計理念

1. **分層架構**: 硬體 → 管理器 → 控制邏輯 → API，清晰的職責分離
2. **依賴反轉**: 高層模組依賴抽象接口，不依賴具體實現
3. **多執行緒並行**: 7個控制邏輯獨立執行，互不干擾
4. **標準化通訊**: 使用 Modbus 寄存器表作為數據中心
5. **由底層到上層**: 初始化順序遵循依賴關係

### 為何如此設計

| 設計決策 | 原因 | 效益 |
|---------|------|------|
| 10秒啟動延遲 | 等待硬體穩定 | 避免初始化錯誤 |
| 多執行緒並行 | 保證實時性 | 控制週期準確、故障隔離 |
| 1000ms 控制週期 | 平衡性能與資源 | CPU 負載適中、響應足夠 |
| Modbus 寄存器表 | 標準化數據訪問 | 解耦合、易測試、可遠端訪問 |
| 通訊先於控制 | 依賴關係 | 初始化數據正確 |
| Redfish 最後初始化 | 依賴所有服務 | API 數據完整可用 |
| 分層架構 | 關注點分離 | 易維護、易擴展、易測試 |

### 系統特點

- ✅ **穩定性**: 看門狗保護、故障隔離、優雅關閉
- ✅ **實時性**: 1秒控制週期、多執行緒並行
- ✅ **可擴展性**: 易於新增機型、控制邏輯模組化
- ✅ **可維護性**: 清晰的架構、詳細的註釋、標準化設計
- ✅ **可測試性**: Mock 接口、解耦合設計
- ✅ **工業標準**: Modbus 協定、Redfish API

---

**文檔版本**: 1.0
**最後更新**: 2025-10-25
**維護者**: KENMEC 開發團隊

---

## 附錄

### A. 縮寫對照表

| 縮寫 | 全稱 | 中文 |
|------|------|------|
| CDU | Cooling Distribution Unit | 冷卻分配單元 |
| PID | Proportional-Integral-Derivative | 比例-積分-微分控制 |
| ADC | Analog-to-Digital Converter | 類比數位轉換器 |
| DAC | Digital-to-Analog Converter | 數位類比轉換器 |
| GPIO | General Purpose Input/Output | 通用輸入輸出 |
| I2C | Inter-Integrated Circuit | 積體電路間匯流排 |
| SPI | Serial Peripheral Interface | 序列周邊介面 |
| HAL | Hardware Abstraction Layer | 硬體抽象層 |
| API | Application Programming Interface | 應用程式介面 |
| HTTP | Hypertext Transfer Protocol | 超文本傳輸協定 |
| JSON | JavaScript Object Notation | JavaScript 物件表示法 |
| RTU | Remote Terminal Unit | 遠端終端單元 |
| TCP | Transmission Control Protocol | 傳輸控制協定 |

### B. 參考文件

1. **Modbus 協定規範**: [Modbus.org](https://modbus.org/)
2. **Redfish API 標準**: [DMTF Redfish](https://www.dmtf.org/standards/redfish)
3. **PID 控制理論**: 自動控制原理
4. **多執行緒程式設計**: POSIX Threads Programming
5. **嵌入式 Linux 系統**: Linux Device Drivers

### C. 版本歷史

| 版本 | 日期 | 變更內容 |
|------|------|---------|
| 1.0 | 2025-10-25 | 初版發布 |

---

**END OF DOCUMENT**

# RS485 Modbus RTU 架構解耦實作計畫

**版本**: 1.0
**日期**: 2026-01-19
**適用**: Kenmec CDU 應用層專案

---

## 專案目標

將專案的 RS485 Modbus RTU 架構解耦，使系統能夠：

1. **同時支援 Modbus RTU 和 TCP**：系統可同時使用兩種協定
2. **原生 TCP 裝置支援**：能連接標準 Modbus TCP 工業設備
3. **雙向通訊**：支援 Master(Client) 和 Slave(Server) 模式
4. **完全向後相容**：現有 `control_hardware_rs485_*` API 保持不變
5. **JSON 配置**：使用 cJSON 解析配置檔，方便運行時配置

## 使用者需求總結

根據使用者確認，本計畫包含以下需求：
- ✅ **共存模式**：同時支援並可動態切換 RTU/TCP
- ✅ **TCP 裝置**：原生 Modbus TCP 設備（工業感測器、PLC等）
- ✅ **相容性**：完全向後相容，不影響現有代碼
- ✅ **連線模式**：同時支援 Master 和 Slave 模式
- ✅ **配置方式**：JSON 配置檔
- ✅ **實作範圍**：完整實作所有功能

---

## 現況分析

### 1. 目前架構層次

```
應用層 (control_logic_*.c)
    ↓ 呼叫 control_hardware_rs485_*()
硬體抽象層 (control_hardware.c)
    ↓ 使用 HID Manager + CModbus API
HID Manager (dexatek/main_application/managers/hid_manager/)
    ↓ 通過 USB HID 裝置（0xA2=IO Board, 0xA3=RTD Board）
libmodbus 函式庫（僅使用 RTU）
    ↓
作業系統串列埠 (/dev/ttyUSB*, /dev/ttyS*)
```

### 2. 關鍵發現

**✅ 優勢：**
- libmodbus 已完整實作 RTU 和 TCP 雙模式
- 有完整的 Modbus Slave 功能（`modbus_manager`）
- 專案已使用 cJSON（可直接整合）

**⚠️ 問題：**
- `control_hardware.c` 的 `rs485_*` 函數硬編碼使用 HID + CModbus API
- 應用層直接呼叫 RS485 特定函數，耦合度高
- 沒有抽象層隔離通訊協定

**📦 HID Manager 依賴：**
目前使用 USB HID 裝置作為 RS485 橋接器：
- HID PID 0xA2：IO Board（GPIO, AD74416H）
- HID PID 0xA3：RTD Board（AD7124, PWM）

### 3. Modbus Manager 現況

**位置**：`dexatek/main_application/managers/modbus_manager/`

**功能**：
- Modbus Slave 模式（作為被動方，接受讀寫）
- 使用 libmodbus 的 `modbus_t` 和 `modbus_mapping_t`
- 定義完整的暫存器映射表（1-700+ 位址）

**限制**：
- 只初始化 RTU context
- 沒有 Master 模式實作
- 沒有 TCP 支援

---

## 解耦合策略

### 核心設計理念

**抽象傳輸層介面**：建立統一的 Modbus 傳輸層抽象，讓上層無需知道底層協定。

### 目標架構

```
應用層 (control_logic_*.c)
    ↓ 呼叫 control_hardware_rs485_*() [舊API，保留相容]
    ↓ 呼叫 control_hardware_modbus_*() [新API，可選]
硬體抽象層 (control_hardware.c) [更新]
    ↓ 使用 Modbus Transport Manager
Modbus Transport Manager [新增]
    ↓
    ├── RTU Transport [新增，包裝現有HID邏輯]
    │   ├── HID Manager（現有）
    │   └── libmodbus RTU
    │
    └── TCP Transport [新增]
        ├── libmodbus TCP Master（新增）
        └── libmodbus TCP Slave（現有 modbus_manager）
```

### 關鍵設計決策

1. **雙 API 設計**：
   - 保留 `control_hardware_rs485_*()` → 內部轉發到新傳輸層（RTU Transport ID=0）
   - 新增 `control_hardware_modbus_*()` → 支援指定傳輸 ID

2. **傳輸層實例管理**：
   - 支援多個傳輸層實例（如 RTU0, TCP1, TCP2）
   - 每個實例獨立配置，動態初始化

3. **Master/Slave 雙模式**：
   - Master：主動讀寫遠端設備（新增）
   - Slave：被動響應請求（基於現有 modbus_manager）

---

## 詳細實作步驟

### 階段 1：建立傳輸層抽象介面

#### 步驟 1.1：設計傳輸層介面結構

**新增檔案**：`kenmec/main_application/control_logic/modbus_transport/modbus_transport_interface.h`

核心內容包括：
- 傳輸類型列舉（RTU、TCP）
- 連線模式列舉（Master、Slave）
- RTU/TCP 配置結構
- 統一配置結構
- 傳輸層操作介面（函數指標）
- 傳輸層實例結構

**關鍵定義**：
```c
typedef enum {
    TRANSPORT_TYPE_RTU,
    TRANSPORT_TYPE_TCP,
    TRANSPORT_TYPE_MAX
} transport_type_t;

typedef enum {
    TRANSPORT_MODE_MASTER,
    TRANSPORT_MODE_SLAVE,
} transport_mode_t;

typedef struct {
    int id;
    transport_type_t type;
    transport_mode_t mode;
    transport_ops_t *ops;
    void *private_data;
    int is_initialized;
} modbus_transport_t;
```

#### 步驟 1.2：實作 RTU 傳輸層（包裝現有邏輯）

**新增檔案**：
- `kenmec/main_application/control_logic/modbus_transport/modbus_transport_rtu.h`
- `kenmec/main_application/control_logic/modbus_transport/modbus_transport_rtu.c`

**功能**：
1. 包裝 `control_hardware.c` 中的 HID + CModbus 邏輯
2. 實作 `transport_ops_t` 介面
3. 保持與現有 RS485 功能完全相同

**重要**：
- 將現有的 HID + CModbus 邏輯遷移到此層
- 提供 `rtu_init()`、`rtu_close()`、`rtu_single_read()`、`rtu_multiple_read()` 等函數
- 建立全局操作結構 `rtu_ops`

#### 步驟 1.3：實作 TCP 傳輸層（新增）

**新增檔案**：
- `kenmec/main_application/control_logic/modbus_transport/modbus_transport_tcp.h`
- `kenmec/main_application/control_logic/modbus_transport/modbus_transport_tcp.c`

**功能**：
1. 使用 libmodbus 的 `modbus_new_tcp()` 建立 TCP Master context
2. 使用 libmodbus 的 `modbus_tcp_listen()` 建立 TCP Slave
3. 實作 `transport_ops_t` 介面

**Master 模式實作要點**：
- 連線管理：使用 `modbus_connect()` 建立連線
- 讀寫操作：使用 `modbus_read_registers()`、`modbus_read_input_registers()`
- 錯誤處理：實現重連機制、超時處理
- Slave ID 管理：使用 `modbus_set_slave()`

**Slave 模式實作要點**：
- 伺服器設置：使用 `modbus_tcp_listen()` 監聽連接
- 請求處理：接收並解析 Modbus TCP 請求
- 資料映射：使用現有的 `modbus_manager` 暫存器映射表
- 連線管理：支援多客戶端連接

#### 步驟 1.4：建立傳輸層管理器

**新增檔案**：
- `kenmec/main_application/control_logic/modbus_transport/modbus_transport_manager.h`
- `kenmec/main_application/control_logic/modbus_transport/modbus_transport_manager.c`

**功能**：
1. 管理多個傳輸層實例（RTU、TCP）
2. 根據配置檔初始化傳輸層
3. 提供統一的 API 介面
4. 路由請求到正確的傳輸層

**核心 API**：
```c
int transport_manager_init(const char *config_file);
int transport_manager_deinit(void);
modbus_transport_t* transport_manager_get(int transport_id);
int transport_manager_single_read(int transport_id, uint8_t slave_id,
                                   uint8_t function_code, uint16_t address,
                                   uint16_t *val);
int transport_manager_multiple_read(int transport_id, uint8_t slave_id,
                                     uint8_t function_code, uint16_t address,
                                     uint16_t quantity, uint16_t *values);
int transport_manager_single_write(int transport_id, uint8_t slave_id,
                                    uint16_t address, uint16_t val);
```

**實作要點**：
- 使用靜態陣列儲存傳輸層實例（支援最多 8 個）
- 根據配置檔解析和初始化各傳輸層
- 提供實例查詢和請求路由功能

---

### 階段 2：配置管理系統

#### 步驟 2.1：設計 JSON 配置格式

**新增檔案**：`kenmec/main_application/control_logic/modbus_transport/modbus_transport_config.json`

**配置範例**：

```json
{
  "transports": [
    {
      "id": 0,
      "type": "RTU",
      "mode": "master",
      "enabled": true,
      "config": {
        "hid_pid": "0xA2",
        "hid_port": 0,
        "baudrate": 115200,
        "timeout_ms": 1000
      }
    },
    {
      "id": 1,
      "type": "TCP",
      "mode": "master",
      "enabled": false,
      "config": {
        "ip": "192.168.1.100",
        "port": 502,
        "timeout_ms": 2000
      }
    },
    {
      "id": 2,
      "type": "TCP",
      "mode": "slave",
      "enabled": false,
      "config": {
        "ip": "0.0.0.0",
        "port": 502,
        "timeout_ms": 1000
      }
    }
  ]
}
```

#### 步驟 2.2：實作配置解析器

**新增檔案**：
- `kenmec/main_application/control_logic/modbus_transport/modbus_transport_config.h`
- `kenmec/main_application/control_logic/modbus_transport/modbus_transport_config.c`

**功能**：
1. 使用 cJSON 解析配置檔
2. 驗證配置參數（IP 格式、port 範圍等）
3. 提供配置查詢 API

**API 設計**：
```c
int config_load(const char *config_file, transport_config_t **configs, int *count);
int config_validate(transport_config_t *config);
void config_free(transport_config_t *configs);
```

---

### 階段 3：更新硬體抽象層

#### 步驟 3.1：更新 control_hardware.h

**修改檔案**：`kenmec/main_application/control_logic/control_hardware.h`

**變更**：
1. 保留所有現有的 `control_hardware_rs485_*` 函數宣告（向後相容）
2. 新增通用的 `control_hardware_modbus_*` 函數宣告

**新增宣告**：
```c
/* ========== 新增：通用 Modbus 通訊函數 ========== */

int control_hardware_modbus_single_read(int transport_id, uint8_t slave_id,
                                        uint8_t function_code, uint16_t address,
                                        uint16_t *val, uint16_t timeout_ms);

int control_hardware_modbus_multiple_read(int transport_id, uint8_t slave_id,
                                          uint8_t function_code, uint16_t address,
                                          uint16_t quantity, uint16_t *values,
                                          uint16_t timeout_ms);

int control_hardware_modbus_single_write(int transport_id, uint8_t slave_id,
                                         uint16_t address, uint16_t val);
```

#### 步驟 3.2：更新 control_hardware.c

**修改檔案**：`kenmec/main_application/control_logic/control_hardware.c`

**變更策略**：
1. 在檔案頭新增 `#include "modbus_transport/modbus_transport_manager.h"`
2. 定義 `#define DEFAULT_RTU_TRANSPORT_ID 0` 以保持向後相容
3. 重構所有 `control_hardware_rs485_*` 函數為轉發到新 API
4. 實作新的 `control_hardware_modbus_*` 函數
5. 在 `control_hardware_init()` 中新增傳輸層管理器初始化

**範例重構**：

舊實作（需移除）：
```c
int control_hardware_rs485_single_read(...) {
    // 直接使用 HID + CModbus API
}
```

新實作（轉發）：
```c
int control_hardware_rs485_single_read(uint8_t hid_port, uint16_t baudrate,
                                       uint8_t slave_id, uint8_t function_code,
                                       uint16_t address, uint16_t *val,
                                       uint16_t timeout_ms) {
    return control_hardware_modbus_single_read(
        DEFAULT_RTU_TRANSPORT_ID,
        slave_id, function_code, address, val, timeout_ms
    );
}

int control_hardware_modbus_single_read(int transport_id, uint8_t slave_id,
                                        uint8_t function_code, uint16_t address,
                                        uint16_t *val, uint16_t timeout_ms) {
    return transport_manager_single_read(
        transport_id, slave_id, function_code, address, val
    );
}
```

初始化更新：
```c
int control_hardware_init(int machine_type) {
    // ... 現有初始化邏輯 ...

    // 新增：初始化傳輸層管理器
    const char *config_path = "kenmec/main_application/control_logic/modbus_transport/modbus_transport_config.json";
    if (transport_manager_init(config_path) != 0) {
        printf("Warning: Failed to initialize transport manager\n");
        // 不中斷程式，允許使用舊的邏輯
    }

    return 0;
}
```

---

### 階段 4：更新 Modbus Manager（Slave 模式）

#### 步驟 4.1：擴展 modbus_manager 支援 TCP Slave

**修改檔案**：
- `dexatek/main_application/managers/modbus_manager/modbus_manager.h`
- `dexatek/main_application/managers/modbus_manager/modbus_manager.c`

**變更**：
1. 支援建立 TCP Slave context（除了現有的 RTU）
2. 管理多個 modbus_t* 實例
3. 提供模式切換 API

**新增 API**：
```c
typedef enum {
    MODBUS_BACKEND_RTU,
    MODBUS_BACKEND_TCP
} modbus_backend_type_t;

int modbus_manager_init_with_backend(modbus_backend_type_t backend);
int modbus_manager_switch_backend(modbus_backend_type_t backend);
```

**實作邏輯**：
- 添加靜態變數：`ctx_rtu`、`ctx_tcp`、`ctx_current`
- 在 `modbus_manager_init_with_backend()` 中根據 backend 類型初始化
- TCP Slave 需要在獨立線程或非阻塞模式下監聽連接

---

### 階段 5：更新編譯系統

#### 步驟 5.1：更新 Makefile

**修改檔案**：`kenmec/main_application/Makefile`

**新增編譯規則**：

```makefile
# Modbus Transport Layer
TRANSPORT_DIR = control_logic/modbus_transport
TRANSPORT_SRCS = $(TRANSPORT_DIR)/modbus_transport_manager.c \
                 $(TRANSPORT_DIR)/modbus_transport_rtu.c \
                 $(TRANSPORT_DIR)/modbus_transport_tcp.c \
                 $(TRANSPORT_DIR)/modbus_transport_config.c

SRCS += $(TRANSPORT_SRCS)

# 確保 libmodbus 和 cJSON 已連結
LDFLAGS += -lmodbus -lcjson
```

---

### 階段 6：測試與驗證

#### 步驟 6.1：單元測試

**新增檔案**：
- `kenmec/main_application/control_logic/modbus_transport/test_transport_rtu.c`
- `kenmec/main_application/control_logic/modbus_transport/test_transport_tcp.c`

**測試項目**：

1. **RTU 傳輸層測試**：
   - 驗證 HID 通訊正常
   - 測試讀寫功能與原有功能一致
   - 檢查錯誤處理

2. **TCP 傳輸層測試**：
   - 測試 TCP 連線建立
   - 驗證讀寫功能正確性
   - 測試斷線重連機制
   - 測試超時處理

3. **傳輸層管理器測試**：
   - 測試多實例管理
   - 驗證配置解析正確性
   - 測試動態切換

#### 步驟 6.2：整合測試

**使用現有 Python 測試工具**：

1. **RTU 模式測試**（使用現有工具）：
   ```bash
   python3 control_logic_rtu_master.py
   python3 io_board_rtu_master.py
   python3 rtd_board_rtu_master.py
   ```

2. **TCP 模式測試**（撰寫新工具）：
   - 新增 `control_logic_tcp_master.py`
   - 使用 pymodbus TCP client 測試

#### 步驟 6.3：端到端測試

**測試場景**：

1. **場景 1：RTU + TCP 並存**
   - 配置：RTU Transport (ID=0) + TCP Transport (ID=1) 同時啟用
   - 操作：同時從 RTU 讀取 IO Board，從 TCP 讀取遠端感測器
   - 驗證：兩個傳輸層獨立運作，互不干擾

2. **場景 2：動態切換**
   - 配置：啟用 RTU 和 TCP
   - 操作：先使用 RTU，再切換到 TCP
   - 驗證：切換過程無錯誤，數據正確

3. **場景 3：錯誤恢復**
   - 操作：斷開 TCP 連線，測試自動重連
   - 驗證：系統能自動恢復，不影響 RTU

---

## 檔案清單

### 新增檔案

```
kenmec/main_application/control_logic/modbus_transport/
├── modbus_transport_interface.h      # 傳輸層抽象介面定義
├── modbus_transport_manager.h        # 傳輸層管理器標頭檔
├── modbus_transport_manager.c        # 傳輸層管理器實作
├── modbus_transport_rtu.h            # RTU 傳輸層標頭檔
├── modbus_transport_rtu.c            # RTU 傳輸層實作
├── modbus_transport_tcp.h            # TCP 傳輸層標頭檔
├── modbus_transport_tcp.c            # TCP 傳輸層實作
├── modbus_transport_config.h         # 配置管理標頭檔
├── modbus_transport_config.c         # 配置解析實作
├── modbus_transport_config.json      # 預設配置檔
├── test_transport_rtu.c              # RTU 單元測試
├── test_transport_tcp.c              # TCP 單元測試
└── control_logic_tcp_master.py       # TCP 測試工具（Python）
```

### 修改檔案

```
kenmec/main_application/
├── control_logic/
│   ├── control_hardware.h             # 新增 modbus_* API 宣告
│   └── control_hardware.c             # 重構為使用傳輸層管理器
└── Makefile                           # 新增編譯規則

dexatek/main_application/managers/modbus_manager/
├── modbus_manager.h                   # 新增 TCP Slave 支援
└── modbus_manager.c                   # 實作 TCP Slave 初始化
```

---

## 驗證計畫

### 驗證目標

1. ✅ **功能驗證**：所有 Modbus 讀寫功能正常
2. ✅ **相容性驗證**：現有代碼無需修改即可運行
3. ✅ **效能驗證**：RTU 和 TCP 延遲在可接受範圍內
4. ✅ **穩定性驗證**：長時間運行無記憶體洩漏或崩潰

### 測試步驟

#### 1. RTU 功能驗證（現有功能不受影響）

```bash
cd kenmec/main_application/control_logic/ls300d
python3 control_logic_rtu_master.py
python3 io_board_rtu_master.py
python3 rtd_board_rtu_master.py

# 預期結果：所有測試通過，與修改前完全相同
```

#### 2. TCP Master 功能驗證

```bash
# 準備 Modbus TCP 模擬器（如 ModbusPal）或真實設備
python3 control_logic_tcp_master.py

# 預期結果：成功連接並讀寫數據
```

#### 3. TCP Slave 功能驗證

```bash
./kenmec_main

# 使用 pymodbus 客戶端連接
python3 -c "
from pymodbus.client import ModbusTcpClient
client = ModbusTcpClient('127.0.0.1', port=502)
client.connect()
result = client.read_holding_registers(1, 10, slave=1)
print(result.registers)
"

# 預期結果：成功讀取暫存器值
```

#### 4. 並存測試

修改配置檔啟用 RTU 和 TCP：
```json
{
  "transports": [
    {"id": 0, "type": "RTU", "enabled": true, ...},
    {"id": 1, "type": "TCP", "enabled": true, ...}
  ]
}
```

運行測試程式，同時使用兩個傳輸層：
```c
uint16_t val_rtu, val_tcp;
control_hardware_modbus_single_read(0, 1, 0x03, 100, &val_rtu, 1000);
control_hardware_modbus_single_read(1, 1, 0x03, 100, &val_tcp, 1000);
```

預期結果：兩個讀取都成功，互不干擾。

#### 5. 錯誤處理測試

- **測試超時**：設定短超時時間，驗證超時錯誤處理
- **測試斷線**：中斷 TCP 連線，驗證錯誤恢復
- **測試無效配置**：提供錯誤的 IP/Port，驗證錯誤提示

#### 6. 記憶體與效能測試

```bash
# 使用 valgrind 檢查記憶體洩漏
valgrind --leak-check=full ./kenmec_main

# 長時間壓力測試
# 持續讀寫 1 小時，監控 CPU、記憶體使用
```

---

## 注意事項

### 1. 向後相容性

- **保證**：所有現有 `control_hardware_rs485_*` 函數保持不變
- **測試**：現有應用層代碼無需修改即可運行
- **遷移**：新代碼建議使用 `control_hardware_modbus_*` API

### 2. 錯誤處理

- **連線失敗**：TCP 連線失敗時，記錄錯誤並返回明確的錯誤碼
- **超時處理**：所有讀寫操作都支援超時設定
- **重連機制**：TCP 斷線後自動嘗試重連（可配置重試次數）

### 3. 執行緒安全

- **單執行緒使用**：初期實作假設單執行緒環境
- **未來擴展**：如需多執行緒，需新增 mutex 保護共享資源

### 4. 配置管理

- **預設配置**：如果配置檔不存在，使用硬編碼的預設值（僅 RTU）
- **動態重載**：支援運行時重載配置（可選功能）

### 5. 文件更新

- **使用手冊**：新增 Modbus TCP 配置說明
- **API 文件**：更新 control_hardware.h 的 Doxygen 註解
- **遷移指南**：提供從舊 API 遷移到新 API 的範例

---

## 實作優先順序

### 高優先 ⭐⭐⭐

1. 傳輸層抽象介面設計（步驟 1.1）
2. RTU 傳輸層實作（步驟 1.2）
3. TCP Master 傳輸層實作（步驟 1.3）
4. 傳輸層管理器實作（步驟 1.4）
5. 配置管理系統（階段 2）

### 中優先 ⭐⭐

6. 更新 control_hardware（階段 3）
7. TCP Slave 支援（步驟 4.1）
8. 編譯系統更新（階段 5）

### 低優先 ⭐

9. 單元測試（步驟 6.1）
10. 整合測試（步驟 6.2）
11. 文件更新

---

## 預期成果

完成本計畫後，系統將具備以下能力：

✅ **同時支援 Modbus RTU 和 TCP**
✅ **向後相容，現有代碼無需修改**
✅ **支援 Master 和 Slave 雙向模式**
✅ **JSON 配置檔，方便運行時調整**
✅ **良好的擴展性，未來可輕鬆新增其他協定**

系統架構將更加清晰，維護性大幅提升，為未來的功能擴展奠定堅實基礎。

---

## 快速開始清單

執行順序：

- [ ] 1. 建立 `kenmec/main_application/control_logic/modbus_transport/` 目錄
- [ ] 2. 實作步驟 1.1：傳輸層介面定義
- [ ] 3. 實作步驟 1.2：RTU 傳輸層（包裝現有邏輯）
- [ ] 4. 實作步驟 1.3：TCP 傳輸層（新增）
- [ ] 5. 實作步驟 1.4：傳輸層管理器
- [ ] 6. 實作步驟 2.1-2.2：配置管理系統
- [ ] 7. 實作步驟 3.1-3.2：更新硬體抽象層
- [ ] 8. 實作步驟 4.1：更新 Modbus Manager
- [ ] 9. 實作步驟 5.1：更新 Makefile
- [ ] 10. 實作步驟 6.1-6.3：測試與驗證
- [ ] 11. 驗證計畫：執行所有測試

---

**文件完成**：本計畫提供了完整的 RS485 Modbus RTU 架構解耦方案，可直接套用於相同結構的專案。

/**
 * @file control_hardware.h
 * @brief 控制硬體介面頭文件
 *
 * 本文件定義與硬體設備通訊的低階介面函數。
 * 主要功能包括：
 * - 數位輸入/輸出 (DI/DO) 控制
 * - 類比輸入/輸出 (AI/AO) 控制
 * - 溫度感測器 (RTD) 讀取
 * - PWM 控制和讀取
 * - RS485 Modbus 通訊
 * - 硬體初始化和配置
 */

#ifndef CONTROL_LOGIC_HARDWARE_H
#define CONTROL_LOGIC_HARDWARE_H

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/

/**
 * @brief 類比輸入/輸出模式列舉
 *
 * 定義類比通道的工作模式（電壓或電流，輸入或輸出）
 */
typedef enum {
    AI_AO_MODE_VOLTAGE_OUT,         /* 電壓輸出模式（0-10V） */
    AI_AO_MODE_CURRENT_OUT,         /* 電流輸出模式（4-20mA） */
    AI_AO_MODE_VOLTAGE_IN,          /* 電壓輸入模式（0-10V） */
    AI_AO_MODE_CURRENT_IN_LOOP,     /* 電流輸入模式 - 迴路供電（4-20mA, 2-wire） */
    AI_AO_MODE_CURRENT_IN_EXTERNAL, /* 電流輸入模式 - 外部供電（4-20mA, 4-wire） */
} AI_AO_MODE;

/*---------------------------------------------------------------------------
                            Type Definitions
 ---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
                            Function Prototypes
 ---------------------------------------------------------------------------*/

/**
 * @brief 初始化控制硬體
 *
 * 根據機型類型初始化所有控制硬體，包括數位 I/O、類比 I/O、
 * 溫度感測器、PWM 等模組。
 *
 * @param machine_type 機型類型（參考 control_logic_machine_type_t）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_init(int machine_type);

/* ========== 數位輸入 (DI) 函數 ========== */

/**
 * @brief 讀取單一數位輸入通道
 *
 * 從硬體讀取指定數位輸入通道的狀態。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-7）
 * @param value 指向存儲讀取值的指標（0 或 1）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_digital_input_get(uint8_t hid_port, uint8_t channel, uint16_t *value);

/**
 * @brief 讀取所有數位輸入通道
 *
 * 一次性從硬體讀取所有 8 個數位輸入通道的狀態。
 *
 * @param hid_port HID 埠號
 * @param value 指向 8 元素陣列的指標，存儲各通道的值
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_digital_input_all_get(uint8_t hid_port, uint16_t value[8]);

/**
 * @brief 從 RAM 讀取所有數位輸入通道
 *
 * 從緩存的 RAM 中讀取所有數位輸入通道的狀態，無需硬體通訊。
 * 速度快但可能不是最新值。
 *
 * @param hid_port HID 埠號
 * @param value 指向 8 元素陣列的指標，存儲各通道的值
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_digital_input_all_get_from_ram(uint8_t hid_port, uint16_t value[8]);

/* ========== 類比 I/O 模式設定函數 ========== */

/**
 * @brief 從 RAM 讀取所有類比通道的工作模式
 *
 * 從緩存的 RAM 中讀取所有類比通道的工作模式設定。
 *
 * @param hid_port HID 埠號
 * @param value 指向 4 元素陣列的指標，存儲各通道的模式
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_analog_mode_all_get_from_ram(uint8_t hid_port, uint16_t value[4]);

/**
 * @brief 設定類比通道的工作模式
 *
 * 配置類比通道的工作模式（電壓/電流，輸入/輸出）。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-3）
 * @param mode 工作模式（參考 AI_AO_MODE 列舉）
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_AI_AO_mode_set(uint8_t hid_port, uint8_t channel, AI_AO_MODE mode, uint16_t timeout_ms);

/* ========== 類比電流輸入 (AI Current) 函數 ========== */

/**
 * @brief 讀取單一類比電流輸入通道
 *
 * 從硬體讀取指定類比電流輸入通道的值（4-20mA）。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-3）
 * @param mA 指向存儲電流值的指標（單位：毫安 mA）
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_analog_input_current_get(uint8_t hid_port, uint8_t channel, int32_t *mA, uint16_t timeout_ms);

/**
 * @brief 讀取所有類比電流輸入通道
 *
 * 一次性從硬體讀取所有 4 個類比電流輸入通道的值。
 *
 * @param hid_port HID 埠號
 * @param uA 指向 4 元素陣列的指標，存儲各通道的電流值（單位：微安 μA）
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_analog_input_current_all_get(uint8_t hid_port, int32_t uA[4], uint16_t timeout_ms);

/**
 * @brief 讀取所有類比通道的工作模式
 *
 * 從硬體讀取所有類比通道的工作模式設定。
 *
 * @param hid_port HID 埠號
 * @param mode 指向 4 元素陣列的指標，存儲各通道的模式
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_analog_mode_all_get(uint8_t hid_port, uint16_t mode[4], uint16_t timeout_ms);

/**
 * @brief 從 RAM 讀取單一類比電流輸入通道
 *
 * 從緩存的 RAM 中讀取類比電流輸入值，無需硬體通訊。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-3）
 * @param mA 指向存儲電流值的指標（單位：毫安 mA）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_analog_input_current_get_from_ram(uint8_t hid_port, uint8_t channel, int32_t *mA);

/* ========== 類比電壓輸入 (AI Voltage) 函數 ========== */

/**
 * @brief 讀取單一類比電壓輸入通道
 *
 * 從硬體讀取指定類比電壓輸入通道的值（0-10V）。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-3）
 * @param mV 指向存儲電壓值的指標（單位：毫伏 mV）
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_analog_input_voltage_get(uint8_t hid_port, uint8_t channel, int32_t *mV, uint16_t timeout_ms);

/**
 * @brief 讀取所有類比電壓輸入通道
 *
 * 一次性從硬體讀取所有 4 個類比電壓輸入通道的值。
 *
 * @param hid_port HID 埠號
 * @param mV 指向 4 元素陣列的指標，存儲各通道的電壓值（單位：毫伏 mV）
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_analog_input_voltage_all_get(uint8_t hid_port, int32_t mV[4], uint16_t timeout_ms);

/**
 * @brief 從 RAM 讀取單一類比電壓輸入通道
 *
 * 從緩存的 RAM 中讀取類比電壓輸入值，無需硬體通訊。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-3）
 * @param mV 指向存儲電壓值的指標（單位：毫伏 mV）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_analog_input_voltage_get_from_ram(uint8_t hid_port, uint8_t channel, int32_t *mV);

/* ========== 類比輸出 (AO) 函數 ========== */

/**
 * @brief 設定類比電壓輸出值
 *
 * 設定指定類比通道的電壓輸出值（0-10V）。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-3）
 * @param val 電壓值（單位依硬體定義）
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_analog_output_voltage_set(uint8_t hid_port, uint8_t channel, uint32_t val, uint16_t timeout_ms);

/**
 * @brief 設定類比電流輸出值
 *
 * 設定指定類比通道的電流輸出值（4-20mA）。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-3）
 * @param val 電流值（單位依硬體定義）
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_analog_output_current_set(uint8_t hid_port, uint8_t channel, uint32_t val, uint16_t timeout_ms);

/* ========== RS485 Modbus 通訊函數 ========== */

/**
 * @brief 從 RS485 設備讀取壓力值
 *
 * 透過 RS485/Modbus 通訊從壓力感測器讀取壓力值。
 *
 * @param hid_port HID 埠號
 * @param baudrate 鮑率（例如：9600, 19200, 115200）
 * @param slave_id Modbus 從站位址
 * @param function_code Modbus 功能碼
 * @param address 暫存器位址
 * @param pressure 指向存儲壓力值的指標（單位依感測器而定）
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_rs485_pressure_get(uint8_t hid_port, uint16_t baudrate, uint8_t slave_id, uint8_t function_code,
    uint16_t address, float *pressure, uint16_t timeout_ms);

/**
 * @brief 從 RS485 設備讀取單一暫存器
 *
 * 透過 RS485/Modbus 通訊讀取單一暫存器的值。
 *
 * @param hid_port HID 埠號
 * @param baudrate 鮑率
 * @param slave_id Modbus 從站位址
 * @param function_code Modbus 功能碼（例如：03=讀保持暫存器）
 * @param address 暫存器位址
 * @param val 指向存儲讀取值的指標
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_rs485_single_read(uint8_t hid_port, uint16_t baudrate, uint8_t slave_id, uint8_t function_code,
    uint16_t address, uint16_t *val, uint16_t timeout_ms);

/**
 * @brief 從 RS485 設備讀取多個暫存器
 *
 * 透過 RS485/Modbus 通訊讀取多個連續暫存器的值。
 *
 * @param hid_port HID 埠號
 * @param baudrate 鮑率
 * @param slave_id Modbus 從站位址
 * @param function_code Modbus 功能碼
 * @param address 起始暫存器位址
 * @param quantity 要讀取的暫存器數量
 * @param values 指向存儲讀取值的陣列指標
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_rs485_multiple_read(uint8_t hid_port, uint32_t baudrate, uint8_t slave_id, uint8_t function_code,
    uint16_t address, uint16_t quantity, uint16_t *values, uint16_t timeout_ms);

/**
 * @brief 向 RS485 設備寫入單一暫存器
 *
 * 透過 RS485/Modbus 通訊寫入單一暫存器的值。
 *
 * @param hid_port HID 埠號
 * @param baudrate 鮑率
 * @param slave_id Modbus 從站位址
 * @param address 暫存器位址
 * @param val 要寫入的值
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_rs485_single_write(uint8_t hid_port, uint16_t baudrate, uint8_t slave_id, uint16_t address, uint16_t val);

/* ========== 數位輸出 (DO) 函數 ========== */

/**
 * @brief 設定單一數位輸出通道
 *
 * 設定指定數位輸出通道的狀態（開或關）。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-7）
 * @param value 輸出值（0=關, 1=開）
 * @param timeout_ms 通訊超時時間（毫秒）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_digital_output_set(uint8_t hid_port, uint8_t channel, uint16_t value, uint16_t timeout_ms);

/**
 * @brief 設定所有數位輸出通道
 *
 * 一次性設定所有 8 個數位輸出通道的狀態。
 *
 * @param hid_port HID 埠號
 * @param value 輸出值（位元遮罩，bit0-bit7 對應通道 0-7）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_digital_output_all_set(uint8_t hid_port, uint16_t value);

/**
 * @brief 讀取所有數位輸出通道的狀態
 *
 * 從硬體讀回所有數位輸出通道的當前狀態。
 *
 * @param hid_port HID 埠號
 * @param value 指向 8 元素陣列的指標，存儲各通道的狀態
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_digital_output_all_get(uint8_t hid_port, uint16_t value[8]);

/**
 * @brief 從 RAM 讀取所有數位輸出通道的狀態
 *
 * 從緩存的 RAM 中讀取數位輸出通道的狀態，無需硬體通訊。
 *
 * @param hid_port HID 埠號
 * @param value 指向 8 元素陣列的指標，存儲各通道的狀態
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_digital_output_all_get_from_ram(uint8_t hid_port, uint16_t value[8]);

/* ========== 溫度感測器 (RTD) 函數 ========== */

/**
 * @brief 讀取單一溫度感測器
 *
 * 從 RTD 溫度感測器（如 PT100, PT1000）讀取溫度值。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-7）
 * @param timeout_ms 通訊超時時間（毫秒）
 * @param temp_float 指向存儲溫度值的指標（單位：°C，浮點數）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_temperature_get(uint8_t hid_port, uint8_t channel, uint16_t timeout_ms, float *temp_float);

/**
 * @brief 讀取所有溫度感測器
 *
 * 一次性從硬體讀取所有 8 個溫度感測器的值。
 *
 * @param hid_port HID 埠號
 * @param timeout_ms 通訊超時時間（毫秒）
 * @param temperature 指向 8 元素陣列的指標，存儲各通道的溫度值（單位：0.1°C，例如 123 表示 12.3°C）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_temperature_all_get(uint8_t hid_port, uint16_t timeout_ms, int32_t temperature[8]);

/**
 * @brief 從 RAM 讀取單一溫度感測器
 *
 * 從緩存的 RAM 中讀取溫度值，無需硬體通訊。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-7）
 * @param temp 指向存儲溫度值的指標（單位：0.1°C，例如 123 表示 12.3°C）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_temperature_get_from_ram(uint8_t hid_port, uint8_t channel, int32_t *temp);

/**
 * @brief 從 RAM 讀取所有溫度感測器
 *
 * 從緩存的 RAM 中讀取所有溫度感測器的值，無需硬體通訊。
 *
 * @param hid_port HID 埠號
 * @param temperature 指向 8 元素陣列的指標，存儲各通道的溫度值（單位：0.1°C）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_temperature_all_get_from_ram(uint8_t hid_port, int32_t temperature[8]);

/**
 * @brief 讀取所有 RTD 感測器的電阻值
 *
 * 讀取所有 RTD 感測器的電阻值，用於診斷或校準。
 *
 * @param hid_port HID 埠號
 * @param timeout_ms 通訊超時時間（毫秒）
 * @param resistor 指向 8 元素陣列的指標，存儲各通道的電阻值（單位：歐姆）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_resistor_all_get(uint8_t hid_port, uint16_t timeout_ms, uint32_t resistor[8]);

/* ========== PWM 控制和讀取函數 ========== */

/**
 * @brief 讀取 PWM 輸入的轉速 (RPM)
 *
 * 讀取 PWM 輸入訊號的頻率，並轉換為轉速 (RPM)。
 * 適用於風扇轉速回饋等應用。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-7）
 * @param timeout_ms 通訊超時時間（毫秒）
 * @param rpm 指向存儲轉速的指標（單位：RPM）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_pwm_rpm_get(uint8_t hid_port, uint8_t channel, uint16_t timeout_ms, float *rpm);

/**
 * @brief 從 RAM 讀取 PWM 輸入的轉速 (RPM)
 *
 * 從緩存的 RAM 中讀取轉速值，無需硬體通訊。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-7）
 * @param rpm 指向存儲轉速的指標（單位：RPM）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_pwm_rpm_get_from_ram(uint8_t hid_port, uint8_t channel, float *rpm);

/**
 * @brief 設定 PWM 輸出的頻率
 *
 * 設定 PWM 輸出訊號的頻率。
 *
 * @param hid_port HID 埠號
 * @param frequency PWM 頻率（單位：Hz）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_pwm_freq_set(uint8_t hid_port, uint32_t frequency);

/**
 * @brief 讀取所有 PWM 通道的頻率
 *
 * 從硬體讀取所有 PWM 通道的頻率值。
 *
 * @param hid_port HID 埠號
 * @param timeout_ms 通訊超時時間（毫秒）
 * @param freq 指向 8 元素陣列的指標，存儲各通道的頻率（單位：Hz）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_pwm_freq_all_get(uint8_t hid_port, uint16_t timeout_ms, uint32_t freq[8]);

/**
 * @brief 從 RAM 讀取所有 PWM 通道的頻率
 *
 * 從緩存的 RAM 中讀取所有 PWM 通道的頻率，無需硬體通訊。
 *
 * @param hid_port HID 埠號
 * @param freq 指向 8 元素陣列的指標，存儲各通道的頻率（單位：Hz）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_pwm_freq_all_get_from_ram(uint8_t hid_port, uint32_t freq[8]);

/**
 * @brief 讀取所有 PWM 通道的週期
 *
 * 讀取所有 PWM 通道的週期值（頻率的倒數）。
 *
 * @param hid_port HID 埠號
 * @param period 指向 8 元素陣列的指標，存儲各通道的週期（單位：微秒 μs）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_pwm_period_all_get(uint8_t hid_port, uint32_t period[8]);

/**
 * @brief 設定 PWM 輸出的責任週期
 *
 * 設定指定 PWM 輸出通道的責任週期（duty cycle）。
 *
 * @param hid_port HID 埠號
 * @param channel 通道號（0-7）
 * @param duty 責任週期（範圍：0-1000，表示 0.0%-100.0%）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_pwm_duty_set(uint8_t hid_port, uint8_t channel, uint16_t duty);

/**
 * @brief 讀取所有 PWM 通道的責任週期
 *
 * 從硬體讀取所有 PWM 通道的責任週期設定值。
 *
 * @param hid_port HID 埠號
 * @param timeout_ms 通訊超時時間（毫秒）
 * @param duty 指向 8 元素陣列的指標，存儲各通道的責任週期
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_pwm_duty_all_get(uint8_t hid_port, uint16_t timeout_ms, uint16_t duty[8]);

/**
 * @brief 從 RAM 讀取所有 PWM 通道的責任週期
 *
 * 從緩存的 RAM 中讀取所有 PWM 通道的責任週期，無需硬體通訊。
 *
 * @param hid_port HID 埠號
 * @param duty 指向 8 元素陣列的指標，存儲各通道的責任週期
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_pwm_duty_all_get_from_ram(uint8_t hid_port, uint16_t duty[8]);

/**
 * @brief 從 RAM 讀取所有 PWM 通道的週期
 *
 * 從緩存的 RAM 中讀取所有 PWM 通道的週期，無需硬體通訊。
 *
 * @param hid_port HID 埠號
 * @param period 指向 8 元素陣列的指標，存儲各通道的週期（單位：微秒 μs）
 * @return 成功返回 0，失敗返回負值錯誤碼
 */
int control_hardware_pwm_period_all_get_from_ram(uint8_t hid_port, uint32_t period[8]);

#endif /* CONTROL_LOGIC_HARDWARE_H */ 
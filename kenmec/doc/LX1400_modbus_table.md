# LX1400 Modbus Table

## Board information

| Index | Board type |
|---------|-------------|
| 0 | IO board |
| 1 | IO board |
| 2 | RTD board |
| 3 | RTD board |

## Modbus communication information

- Slave ID: 100
- UART setting: 115200, 8, N, 1

## Modbus table (Common)

| For HMI Address | Function code | Unit | Name | Description | control_logic |
|---------|-------------|------|-----------|---------|---------|
| 1001 | 3/6 | 0/1 | CONTROL_LOGIC_1_ENABLE | 控制邏輯1啟用 | control_logic_1 |
| 1002 | 3/6 | 0/1 | CONTROL_LOGIC_2_ENABLE | 控制邏輯2啟用 | control_logic_2 |
| 1003 | 3/6 | 0/1 | CONTROL_LOGIC_3_ENABLE | 控制邏輯3啟用 | control_logic_3 |
| 1004 | 3/6 | 0/1 | CONTROL_LOGIC_4_ENABLE | 控制邏輯4啟用 | control_logic_4 |
| 1005 | 3/6 | 0/1 | CONTROL_LOGIC_5_ENABLE | 控制邏輯5啟用 | control_logic_5 |
| 1006 | 3/6 | 0/1 | CONTROL_LOGIC_6_ENABLE | 控制邏輯6啟用 | control_logic_6 |
| 2001 | 3/6 | 0/1 | SYSTEM_STATUS | 機組狀態 | control_logic_4 <br> control_logic_5 |
| 2801 | 3/6 | 0/1 | CURRENT_FAIL_COUNT | 補水未滿次數 (當前值) | control_logic_5 |
| 5001 | 3/6 | °C | TEMP_SETPOINT | 目標控制溫度 | control_logic_1 <br> control_logic_4 |
| 5002 | 3/6 | bar | PRESSURE_SETPOINT | 目標控制壓差 | control_logic_2 <br> control_logic_4 |
| 5003 | 3/6 | L/min | FLOW_SETPOINT | 目標控制流量 | control_logic_1 <br> control_logic_3 <br> control_logic_4 <br> control_logic_6 |
| 5005 | 3/6 | 0/1 | CONTROL_MODE | 流量模式/壓差模式 | control_logic_2 <br> control_logic_3 <br> control_logic_4 <br> control_logic_6 |
| 5006 | 3/6 | L/min | FLOW_HIGH_LIMIT | 流量上限 | control_logic_3 <br> control_logic_6 |
| 5007 | 3/6 | L/min | FLOW_LOW_LIMIT | 流量下限 | control_logic_3 <br> control_logic_6 |
| 5020 | 3/6 | 0/1 | TEMP_CONTROL_MODE <br> AUTO_START_STOP | 溫度控制模式 <br> 自動啟動/停止 | control_logic_1 <br> control_logic_4 |
| 5021 | 3/6 | 0/1 | PUMP1_MANUAL | 泵浦1手動模式 | control_logic_1 <br> control_logic_2 <br> control_logic_3 <br> control_logic_4 |
| 5022 | 3/6 | 0/1 | PUMP2_MANUAL | 泵浦2手動模式 | control_logic_1 <br> control_logic_2 <br> control_logic_3 <br> control_logic_4 |
| 5023 | 3/6 | 0/1 | PUMP3_MANUAL | 泵浦3手動模式 | control_logic_1 <br> control_logic_2 <br> control_logic_3 <br> control_logic_4 |
| 5025 | 3/6 | 0/1 | PUMP1_AUTO | 泵浦1停用 | control_logic_4 |
| 5026 | 3/6 | 0/1 | PUMP2_AUTO | 泵浦2停用 | control_logic_4 |
| 5027 | 3/6 | 0/1 | PUMP3_AUTO | 泵浦3停用 | control_logic_4 |
| 5031 | 3/6 | % | PUMP_MIN_SPEED | 泵浦最小速度 | control_logic_4 |
| 5032 | 3/6 | % | PUMP_MAX_SPEED | 泵浦最大速度 | control_logic_4 |
| 5051 | 3/6 | bar | TARGET_PRESSURE | 補水壓力設定 | control_logic_5 |
| 5052 | 3/6 | 0.1s | START_DELAY | 補水啟動延遲 | control_logic_5 |
| 5053 | 3/6 | 0.1s | MAX_RUN_TIME | 補水運行時間超時 | control_logic_5 |
| 5054 | 3/6 | 0.1s | COMPLETE_DELAY | 補水完成延遲 | control_logic_5 |
| 5055 | 3/6 | 0.1s | WARNING_DELAY | 缺水警告延遲 | control_logic_5 |
| 5056 | 3/6 | 0/1 | MAX_FAIL_COUNT | 補水未滿告警次數 | control_logic_5 |
| 5061 | 3/6 | % | VALVE_STATE | 比例閥手動模式<br>電磁閥手動模式 | control_logic_1 <br> control_logic_2 <br> control_logic_3 <br> control_logic_6 |
| 6271 | 3/6 | 0/1 | HIGH_PRESSURE_ALARM | 高壓警報 | control_logic_2 |
| 6272 | 3/6 | 0/1 | HIGH_PRESSURE_SHUTDOWN | 高壓停機 | control_logic_2 |

## Modbus table (AIO boards)

| For HMI Address | Board index | Name | Description | Note | control_logic |
|---------|-------------|-----------|---------|---------|---------|
| 11037 | 0 | PUMP1_SPEED_CMD | AC泵浦1設定轉速 | AO_0_voltage 0-1000, 999 -> 9.99v | control_logic_1 <br> control_logic_2 <br> control_logic_3 <br> control_logic_4 |
| 11039 | 0 | PUMP2_SPEED_CMD | AC泵浦2設定轉速 | AO_1_voltage 0-1000, 999 -> 9.99v | control_logic_1 <br> control_logic_2 <br> control_logic_3 <br> control_logic_4 |
| 11041 | 0 | PUMP3_SPEED_CMD | AC泵浦3設定轉速 | AO_2_voltage 0-1000, 999 -> 9.99v | control_logic_1 <br> control_logic_2 <br> control_logic_3 <br> control_logic_4 |
| 11101 | 1 | PUMP1_RUN_CMD | 泵浦1啟停控制 | DO_0 | control_logic_1 <br> control_logic_2 <br> control_logic_3 <br> control_logic_4 |
| 11103 | 1 | PUMP2_RUN_CMD | 泵浦2啟停控制 | DO_2 | control_logic_1 <br> control_logic_2 <br> control_logic_3 <br> control_logic_4 |
| 11105 | 1 | PUMP3_RUN_CMD | 泵浦3啟停控制 | DO_4 | control_logic_1 <br> control_logic_2 <br> control_logic_3 <br> control_logic_4 |
| 11102 | 1 | PUMP1_RESET_CMD | 泵浦1異常復歸 | DO_1 | control_logic_4 |
| 11104 | 1 | PUMP2_RESET_CMD | 泵浦2異常復歸 | DO_3 | control_logic_4 |
| 11106 | 1 | PUMP3_RESET_CMD | 泵浦3異常復歸 | DO_5 | control_logic_4 |
| 11108 | 1 | WATER_PUMP_CONTROL | 補水泵啟停控制 | DO_7 | control_logic_5 |
| 11109 | 1 | PUMP1_FAULT  | 泵浦1過載狀態 | DI_0 | control_logic_4 |
| 11110 | 1 | PUMP2_FAULT  | 泵浦2過載狀態 | DI_1 | control_logic_4 |
| 11111 | 1 | PUMP3_FAULT  | 泵浦3過載狀態 | DI_2 | control_logic_4 |
| 11112 | 1 | HIGH_LEVEL | CDU水箱_高液位檢 | DI_3 | control_logic_5 |
| 11113 | 1 | LOW_LEVEL | CDU水箱_低液位檢 | DI_4 | control_logic_5 |
| 11114 | 1 | LEAK_DETECTION | 漏液檢 | DI_5 | control_logic_5 |
| 11147 | 1 | VALVE_COMMAND | 電磁閥命令開度設定值 | AO_1_current | control_logic_1 <br> control_logic_2 <br> control_logic_3 <br> control_logic_6 |
| 11161 | 1 | VALVE_STATE | 電磁閥開度回饋值 | AI_0_current | control_logic_2 <br> control_logic_3 <br> control_logic_6 |
| 14554 | 3 | T11_TEMP | T11進水溫度 | RTD_0 | control_logic_1 |
| 14556 | 3 | T12_TEMP | T12進水溫度 | RTD_1 | control_logic_1 |
| 14566 | 3 | T17_TEMP | T17出水溫度 | RTD_6 | control_logic_1 |
| 14568 | 3 | T18_TEMP | T18出水溫度 | RTD_7 | control_logic_1 |

## Modbus table (Modbus device)

| For HMI Address | Slave ID | Function code | Address | Unit | Name | Description | control_logic |
|---------|-------------|------|-----------|---------|---------|---------|---------|
| 2021 | 1 | 3 | 0 | °C | Ambient Temperature | 環境溫度 | |
| 2022 | 1 | 3 | 1 | % | Ambient Humidity | 環境濕度 | |
| 2062 | 2 | 3 | 0 | L/min | F1 | 一次側進水流量 | control_logic_3 <br> control_logic_4 |
| 2063 | 3 | 3 | 0 | L/min | F2 | 二次側出水流量 | control_logic_1 <br> control_logic_3 <br> control_logic_4 <br> control_logic_6 |
| 2064 | 4 | 3 | 0 | L/min | F3 | 二次側進水流量 | control_logic_3 <br> control_logic_4 |
| 2065 | 5 | 3 | 0 | L/min | F4 | 一次側出水流量 | control_logic_3 <br> control_logic_4 |
| 2082 | 6 | 3 | 0 | bar | P1 | 一次側進水壓力 | control_logic_2 |
| 2083 | 7 | 3 | 0 | bar | P2 | 一次側進水壓力 | |
| 2084 | 8 | 3 | 0 | bar | P3 | 一次側過濾器後壓力 | |
| 2085 | 9 | 3 | 0 | bar | P4 | 一次側過濾器後壓力 | |
| 2086 | 10 | 3 | 0 | bar | P5 | 一次側進板熱壓力 | |
| 2087 | 11 | 3 | 0 | bar | P6 | 一次側進板熱壓力 | |
| 2088 | 12 | 3 | 0 | bar | P7 | 一次側出板熱壓力 | |
| 2089 | 13 | 3 | 0 | bar | P8 | 一次側出水壓力 | |
| 2090 | 14 | 3 | 0 | bar | P9 | 一次側出水壓力 | control_logic_2 |
| 2092 | 15 | 3 | 0 | bar | P11 | 二次側進水壓力 | control_logic_2 <br> control_logic_6 |
| 2093 | 16 | 3 | 0 | bar | P12 | 二次側進水壓力 | control_logic_1 <br> control_logic_2 |
| 2094 | 17 | 3 | 0 | bar | P13 | 二次側出板熱壓力 | control_logic_1 |
| 2095 | 18 | 3 | 0 | bar | P14 | 二次側出板熱壓力 | |
| 2096 | 19 | 3 | 0 | bar | P15 | 二次側水泵前壓力 | control_logic_4 |
| 2097 | 20 | 3 | 0 | bar | P16 | 二次側水泵前壓力 | control_logic_2 <br> control_logic_4 |
| 2098 | 21 | 3 | 0 | bar | P17 | 二次側出水壓力 | control_logic_2 <br> control_logic_4 <br> control_logic_6 |
| 2099 | 22 | 3 | 0 | bar | P18 | 二次側出水壓力 | control_logic_2 <br> control_logic_4 |
| 2501 | 23 | 3 | 201 | bar | PUMP1_FREQ  | AC泵浦1輸出頻率 | control_logic_4 |
| 2502 | 23 | 3 | 202 | bar | PUMP1_CURRENT | AC泵浦1電流 (A×0.01) | control_logic_4 |
| 2503 | 23 | 3 | 203 | bar | PUMP1_VOLTAGE | AC泵浦1電壓 (V×0.1) | control_logic_4 |
| 2511 | 24 | 3 | 201 | bar | PUMP2_FREQ  | AC泵浦2輸出頻率 | control_logic_4 |
| 2512 | 24 | 3 | 202 | bar | PUMP2_CURRENT | AC泵浦2電流 (A×0.01) | control_logic_4 |
| 2513 | 24 | 3 | 203 | bar | PUMP2_VOLTAGE | AC泵浦2電壓 (V×0.1) | control_logic_4 |
| 2521 | 25 | 3 | 201 | bar | PUMP3_FREQ  | AC泵浦3輸出頻率 | control_logic_4 |
| 2522 | 25 | 3 | 202 | bar | PUMP3_CURRENT | AC泵浦3電流 (A×0.01) | control_logic_4 |
| 2523 | 25 | 3 | 203 | bar | PUMP3_VOLTAGE | AC泵浦3電壓 (V×0.1) | control_logic_4 |
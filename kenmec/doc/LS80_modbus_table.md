# LS80 Modbus Table

# Board information

| USB slot | Board type |
|---------|-------------|
| 0 | AI/O board |
| 1 | AI/O board |
| 2 | RTD/PWM board |

## Control logic 1 Modbus table

| Address | Function code | Description | Unit | Data Type | R/W | Default | Note |
|---------|-------------|------|-----------|-----|---------|-----|-----|
| 1001 | 03 | CONTROL_LOGIC_1_ENABLE | N/A | bool | R/W | 0 | 控制邏輯1啟用 |
| 2063 | 03 | F2 | L/min | uint16 | R | 0 | 流量 |
| 2085 | 03 | P4 | bar | uint16 | R | 0 | 壓力 |
| 2094 | 03 | P13 | bar | uint16 | R | 0 | 壓力 |
| 5001 | 03/06 | TARGET_TEMP | 0.1°C | uint16 | R/W | 0 | 目標溫度 |
| 5003 | 03/06 | FLOW_SETPOINT | L/min | uint16 | R/W | 0 | 流量 |
| 5015 | 03/06 | PUMP1_SPEED | 0-100 | uint16 | R/W | 0 | Pump1速度 |
| 5016 | 03/06 | PUMP2_SPEED | 0-100 | uint16 | R/W | 0 | Pump2速度 |
| 5020 | 03/06 | TEMP_CONTROL_MODE | 0/1 | uint16 | R/W | 0 | 溫度控制模式 |
| 5021 | 03/06 | PUMP1_MANUAL_MODE | 0/1 | uint16 | R/W | 0 | Pump1手動模式 |
| 5022 | 03/06 | PUMP2_MANUAL_MODE | 0/1 | uint16 | R/W | 0 | Pump2手動模式 |
| 5061 | 03/06 | VALVE_MANUAL_MODE | 0/1 | uint16 | R/W | 0 | 比例閥手動模式 |
| 13560 | 03 | T4 | 0.1°C | uint32 | R | 0 | 入水溫度 |
| 13556 | 03 | T2 | 0.1°C | uint32 | R | 0 | 出水溫度 |
| 11147 | 06 | VALVE_OPENING | % | uint16 | W | 0 | 比例閥開度 |
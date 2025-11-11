# LA12S Modbus Table

## Board information

| USB slot | Board type |
|---------|-------------|
| 0 | I/O board |
| 1 | RTD/PWM board |

## Control logic modbus table

| Address | Function code | Description | Unit | Data Type | R/W | Default | Note |
|---------|-------------|------|-----------|-----|---------|-----|-----|
| 2 | 03 | 警報顯示 | N/A | uint16 | R | 0 | bit 4: 內部回水T2a溫度過低<br>bit 5: 內部回水T2a溫度過高<br>bit 6: 內部回水T2b溫度過低<br>bit 7: 內部回水T2b溫度過高<br>bit 8: 溫度差異警報(>1.5°C) |
| 11 | 03 | 水箱出水平均溫度 T2 avg | 0.1°C | uint16 | R | 0 | 
| 14 | 03 | 伺服器冷卻入水溫度 T3 avg | 0.1°C | uint16 | R | 0 |
| 17 | 03 | 水箱回水平均溫度 T4 avg | 0.1°C | uint16 | R | 0 |

## Mapping to I/O board modbus table

| Address | Function code | Description | Unit | Data Type | R/W | Default | Note |
|---------|-------------|------|-----------|-----|-----|-----|-----|
| 11001 | 03/06 | 變頻器[1] 啟動 Y15 (DO_0) | N/A | uint16 | R/W | 0 | |
| 11002 | 03/06 | 變頻器[1] 異常Reset Y16 (DO_1) | N/A | uint16 | R/W | 0 | |
| 11009 | 03 | 變頻器[1] 異常 X16 (DI_0) | N/A | uint16 | R | 0 | |
| 11010 | 03 | 二次側水箱液位開關 X12 (DI_1) | N/A | uint16 | R | 0 | |
| 11011 | 03 | 二次側水箱液位開關 X14 (DI_2) | N/A | uint16 | R | 0 | |
| 11012 | 03 | 盛水盤漏液偵測 X13 (DI_3) | N/A | uint16 | R | 0 | |
| 11013 | 03 | 補水箱液位檢(on:無水,off:有水)b接點 X15 (DI_4) | N/A | uint16 | R | 0 | |
| 11003 | 03/06 | 補水泵輸出 Y17 (DO_2) | N/A | uint16 | R/W | 0 | |
| 12554 | 03 | 水箱出水溫度 T2a (RTD_0) | ohm | uint32 | R | 0 | |
| 12556 | 03 | 水箱出水溫度 T2b (RTD_1) | ohm | uint32 | R | 0 | |
| 12558 | 03 | 伺服器冷卻入水溫度 T3a (RTD_2) | ohm | uint32 | R | 0 | |
| 12560 | 03 | 伺服器冷卻入水溫度 T3b (RTD_3) | ohm | uint32 | R | 0 | |
| 12562 | 03 | 水箱回水平均溫度 T4a (RTD_4) | ohm | uint32 | R | 0 | |
| 12564 | 03 | 水箱回水平均溫度 T4b (RTD_5) | ohm | uint32 | R | 0 | |
| 15000 | 03/06 | Control logic 1 enable/disable | N/A | bool | R/W | 0 | 0: disable, 1: enable |
| 15001 | 03/06 | Control logic 1 fan rotation config | N/A | uint16 | R/W | 2 | pulse per rotation |
| 15002 | 03/06 | Control logic 1 target coolant temperature | 0.1°C | uint16 | R/W | 250 | TARGET_COOLANT_TEMP |
| 15003 | 03/06 | Control logic 1 tolerance | 0.1°C | uint16 | R/W | 20 | TEMP_TOLERANCE |
| 15004 | 03/06 | Control logic 1 critical high | 0.1°C | uint16 | R/W | 350 | TEMP_CRITICAL_HIGH |
| 15005 | 03/06 | Control logic 1 critical low | 0.1°C | uint16 | R/W | 150 | TEMP_CRITICAL_LOW |
| 15006 | 03/06 | Control logic 1 moving average windows size | N/A | uint16 | R/W | 10 | sample/200ms |

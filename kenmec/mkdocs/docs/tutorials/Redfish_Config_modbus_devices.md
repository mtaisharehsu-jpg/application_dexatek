# Tutorial: OEM Kenmec Config Read/Write API for Modbus device configs.

## Overview

This API is used to read/write modbus device configs, after modbus device configs is set, the value will be updated to target address.

## 1. Read example

### Endpoint

```bash
GET /redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Read
```

### Request example

```curl
curl --location '192.168.0.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Read'
```

### Response example

```json
{
    "@odata.type": "#KenmecConfig.v1_0_0.KenmecConfig",
    "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Read",
    "ModbusDeviceConfigs": [
        { "board": 2, "baudrate": 9600, "slave_id": 2, "code": 3, "address": 0, "data_type": 1, "scale": 1, "update_address": 2062, "name": "F1" },
        { "board": 2, "baudrate": 9600, "slave_id": 3, "code": 3, "address": 0, "data_type": 1, "scale": 1, "update_address": 2063, "name": "F2" }
    ]
}
```

| Attribute | Description  | Data type | Data Description |
| --------- | -------------| --------- | -----------------|
| odata.type | odata type | string | |
| odata.id | odata id | string | |
| ModbusDeviceConfigs | Modbus device configs | array | |
| board | Board index | int | range: 0 - 3 |
| baudrate | Baud rate | int | range: 9600 - 115200 |
| slave_id | Modbus device slave ID | int | range: 0 - 247 |
| code | Function code | int | 3/4/6 |
| address | Modbus device read address | int |  |
| data_type | data type | int | 0: uint16 <br> 1: int16 <br> 2: int32 <br> 3: uint32 <br> 4: float32 <br> 5: uint16_lowbyte <br> 6: uint16_highbyte |
| scale | value by a scale factor | float | Example: if value is 10 <br> when scale is 2, then the value will be 20 <br> when scale is 0.5, then the value will be 5 |
| update_address | Update value to target address after read | int |  |
| name | config name | string | |

## 2. Write example 1

- Add a config for Modbus device write
!!! example "Modbus device info"
    Board index: 2 <br>
    Slave ID: 10 <br>
    Baud rate: 9600 <br>
    Function code: 6 <br>
    Address: 0 <br>
    Data type: 1 <br>
    Scale: 1 <br>
    Update address: 3000

!!! warning
    Config will replace the existing config, so if you want to add a new item, you need to get the existing config first, then add the new item to the existing config.

### Endpoint

```bash
POST /redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write
```

### Request example

```curl
curl --location '192.168.0.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write' \
--header 'Content-Type: application/json' \
--data '{
    "ModbusDeviceConfigs":[
      {"board":2,"baudrate":9600,"slave_id":2,"code":3,"address":0,"data_type":1,"scale":1,"update_address":2062,"name":"F1"},
      {"board":2,"baudrate":9600,"slave_id":3,"code":3,"address":0,"data_type":1,"scale":1,"update_address":2063,"name":"F2"},
      {"board":2,"baudrate":9600,"slave_id":10,"code":6,"address":0,"data_type":1,"scale":1,"update_address":3000,"name":"WriteTest"}
    ]
}'
```

### Response example

```json
{
  "@odata.type": "#KenmecConfig.v1_0_0.KenmecConfig",
  "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write",
  "Status": "Success"
}
```

## 3. Write example 2

- Add config for Modbus device read with different slave ID and address

!!! example "Modbus device info"
    Board index: 3 <br>
    Baud rate: 9600 <br>
    Slave ID: 4 <br>
    Function code: 3 <br>
    Address: 0 <br>
    Data type: 1 <br>
    Scale: 1 <br>
    Update address: 2064 <br>
    Name: F3

!!! warning "Config"
    Config will replace the existing config, so if you want to add a new item, you need to get the existing config first, then add the new item to the existing config.

### Endpoint

```bash
POST /redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write
```

### Request example

```curl
curl --location '192.168.0.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write' \
--header 'Content-Type: application/json' \
--data '{
    "ModbusDeviceConfigs":[
      {"board":2,"baudrate":9600,"slave_id":2,"code":3,"address":0,"data_type":1,"scale":1,"update_address":2062,"name":"F1"},
      {"board":2,"baudrate":9600,"slave_id":3,"code":3,"address":0,"data_type":1,"scale":1,"update_address":2063,"name":"F2"},
      {"board":3,"baudrate":9600,"slave_id":4,"code":3,"address":0,"data_type":1,"scale":1,"update_address":2064,"name":"F3"},
      {"board":2,"baudrate":9600,"slave_id":10,"code":6,"address":0,"data_type":1,"scale":1,"update_address":3000,"name":"WriteTest"}
    ]
}'
```

### Response example

```json
{
  "@odata.type": "#KenmecConfig.v1_0_0.KenmecConfig",
  "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write",
  "Status": "Success"
}
```

## 4. Cleanup config

- If you want to clean up the config, you can set the ModbusDeviceConfigs to an empty array.

### Endpoint

```bash
POST /redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write
```

### Request example

```curl
curl --location '192.168.0.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write' \
--header 'Content-Type: application/json' \
--data '{
    "ModbusDeviceConfigs":[]
}'
```

### Response example

```json
{
  "@odata.type": "#KenmecConfig.v1_0_0.KenmecConfig",
  "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write",
  "Status": "Success"
}
```
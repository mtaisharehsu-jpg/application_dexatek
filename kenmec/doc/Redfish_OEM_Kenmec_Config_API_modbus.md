# Redfish API: OEM Kenmec Config Read/Write API for Modbus device read/write

## Overview

This API is used to read/write modbus device config, after modbus device config is set, the value will be updated to target address.

## Config read

### Endpoint

```bash
/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Read
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
        { "board": 2, "baudrate": 9600, "slave_id": 2, "code": 3, "address": 0, "data_type": 1, "update_address": 2062, "name": "F1" },
        { "board": 2, "baudrate": 9600, "slave_id": 3, "code": 3, "address": 0, "data_type": 1, "update_address": 2063, "name": "F2" }
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
| data_type | data type | int | 0: uint16, 1: int16, 2: int32, 3: uint32, 4: float32 |
| update_address | Update value to target address | int |  |
| name | config name | string | |

## Config write example 1

!!! example "Add a config for Modbus device write"
    Board index: 2
    Modbus device slave: 10
    Baud rate: 9600
    Function code: 6
    Address: 0
    Data type: 1
    Update address: 3000 (For HMI write address)

!!! warning "Config"
    Config will replace the existing config, so if you want to add a new item, you need to get the existing config first, then add the new item to the existing config.

### Endpoint

```bash
/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write
```

### Request example

```curl
curl --location '192.168.0.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write' \
--header 'Content-Type: application/json' \
--data '{
    "ModbusDeviceConfigs":[
      {"board":2,"baudrate":9600,"slave_id":2,"code":3,"address":0,"data_type":1,"update_address":2062,"name":"F1"},
      {"board":2,"baudrate":9600,"slave_id":3,"code":3,"address":0,"data_type":1,"update_address":2063,"name":"F2"},
      {"board":2,"baudrate":9600,"slave_id":10,"code":6,"address":0,"data_type":1,"update_address":3000,"name":"WriteTest"}
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

## Config write example 2

!!! example "Add config for Modbus device read with different slave ID and address"
    Add board 2, baudrate 9600, slave id 4, code 3, address 0, data type 1, update address 2064, name F3
    Add board 2, baudrate 9600, slave id 5, code 3, address 0, data type 1, update address 2065, name F4

### Endpoint

```bash
/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write
```

### Request example

```curl
curl --location '192.168.0.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write' \
--header 'Content-Type: application/json' \
--data '{
    "ModbusDeviceConfigs":[
      {"board":2,"baudrate":9600,"slave_id":2,"code":3,"address":0,"data_type":1,"update_address":2062,"name":"F1"},
      {"board":2,"baudrate":9600,"slave_id":3,"code":3,"address":0,"data_type":1,"update_address":2063,"name":"F2"},
      {"board":2,"baudrate":9600,"slave_id":4,"code":3,"address":0,"data_type":1,"update_address":2064,"name":"F3"},
      {"board":2,"baudrate":9600,"slave_id":5,"code":3,"address":0,"data_type":1,"update_address":2065,"name":"F4"},
      {"board":2,"baudrate":9600,"slave_id":10,"code":6,"address":0,"data_type":1,"update_address":3000,"name":"WriteTest"}
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

### check current config

```bash
curl --location '192.168.0.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Read'
```

### Response example

```json
{
  "@odata.type": "#KenmecConfig.v1_0_0.KenmecConfig",
  "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Read",
  "ModbusDeviceConfigs": [
    {"board":2,"baudrate":9600,"slave_id":2,"code":3,"address":0,"data_type":1,"update_address":2062,"name":"F1"},
    {"board":2,"baudrate":9600,"slave_id":3,"code":3,"address":0,"data_type":1,"update_address":2063,"name":"F2"},
    {"board":2,"baudrate":9600,"slave_id":4,"code":3,"address":0,"data_type":1,"update_address":2064,"name":"F3"},
    {"board":3,"baudrate":9600,"slave_id":5,"code":3,"address":0,"data_type":1,"update_address":2065,"name":"F4"},
    {"board":2,"baudrate":9600,"slave_id":10,"code":6,"address":0,"data_type":1,"update_address":3000,"name":"WriteTest"}
  ]
}
```

## Cleanup config

### Endpoint

```bash
/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config.Write
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
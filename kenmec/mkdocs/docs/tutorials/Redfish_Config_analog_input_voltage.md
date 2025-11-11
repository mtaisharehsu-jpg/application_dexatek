# Tutorial: OEM Kenmec Config Read/Write API for analog input current configs

## Overview

This API is used to read/write analog input voltage sensors configs, after analog input voltage sensors configs is set, the analog voltage value will covert by sensor type and be updated to target address.

!!! warning "Analog input voltage sensor type"
    Currently, analog input voltage sensor type is not yet defined.

## Config read

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
    "AnalogInputVoltageConfigs": [
        {"board":0, "channel":0, "sensor_type":0, "update_address":1111, "name":"V1"},
        {"board":0, "channel":1, "sensor_type":0, "update_address":2222, "name":"V2"},
    ]
}
```

| Attribute | Description  | Data type | Data Description |
| --------- | -------------| --------- | -----------------|
| odata.type | odata type | string | |
| odata.id | odata id | string | |
| AnalogInputVoltageConfigs | analog input voltage sensor configs | array | |
| board | board index | int | range: 0 - 3 |
| channel | channel index | int | range: 0 - 3 |
| sensor_type | sensor type | int | 0: A sensor (0-10V -> xxx) <br> 1: B sensor (0-10V -> yyy) |
| update_address | update address | int | converted value will be updated to target address |
| name | config name | string | |

## Config write

!!! example "Config analog input current sensors"
    Board index: 0, AI_0, sensor: aaa sensor, update address: 1111, name: V1 <br>
    Board index: 0, AI_1, sensor: bbb sensor, update address: 2222, name: V2 <br>

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
    "AnalogInputVoltageConfigs":[
        {"board":0, "channel":0, "sensor_type":0, "update_address":1111, "name":"V1"},
        {"board":0, "channel":1, "sensor_type":0, "update_address":2222, "name":"V2"}
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

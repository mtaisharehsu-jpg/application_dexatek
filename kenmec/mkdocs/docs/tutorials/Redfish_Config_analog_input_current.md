# Tutorial: OEM Kenmec Config Read/Write API for analog input current configs

## Overview

This API is used to read/write analog input current sensors configs, after analog input current sensors configs is set, the analog current value will covert by sensor type and be updated to target address.

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
    "AnalogInputCurrentConfigs": [
        {"board":0, "channel":0, "sensor_type":0, "update_address":2082, "name":"P1"},
        {"board":0, "channel":1, "sensor_type":0, "update_address":2083, "name":"P2"},
        {"board":0, "channel":2, "sensor_type":0, "update_address":2084, "name":"P3"},
        {"board":0, "channel":3, "sensor_type":0, "update_address":2085, "name":"P4"}
    ]
}
```

| Attribute | Description  | Data type | Data Description |
| --------- | -------------| --------- | -----------------|
| odata.type | odata type | string | |
| odata.id | odata id | string | |
| AnalogInputCurrentConfigs | analog input current sensor configs | array | |
| board | board index | int | range: 0 - 3 |
| channel | channel index | int | range: 0 - 3 |
| sensor_type | sensor type | int | 0: flow sensor (4-20mA -> 0-100LPM) <br> 1: pressure sensor (4-20mA -> 0-10 bar) |
| update_address | update address | int | converted value will be updated to target address |
| name | config name | string | |

## Config write

!!! example "Config analog input current sensors"
    Board index: 0, AI_0, sensor: flow sensor, update address: 2082, name: F1 <br>
    Board index: 0, AI_1, sensor: flow sensor, update address: 2083, name: F2 <br>
    Board index: 0, AI_2, sensor: pressure sensor, update address: 2084, name: P1 <br>
    Board index: 0, AI_3, sensor: pressure sensor, update address: 2085, name: P2 <br>

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
    "AnalogInputCurrentConfigs":[
        {"board":0, "channel":0, "sensor_type":0, "update_address":2082, "name":"F1"},
        {"board":0, "channel":1, "sensor_type":0, "update_address":2083, "name":"F2"},
        {"board":0, "channel":2, "sensor_type":1, "update_address":2084, "name":"P1"},
        {"board":0, "channel":3, "sensor_type":1, "update_address":2085, "name":"P2"}
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

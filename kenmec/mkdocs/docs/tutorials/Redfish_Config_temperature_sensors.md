# Tutorial: OEM Kenmec Config Read/Write API for RTD channel configs

## Overview

This API is used to read/write RTD channel temperature sensors configs, after RTD channel temperature sensors configs is set, the temperature value will be updated to target address.

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
    "TemperatureConfigs": [
        {"board":2, "channel":0, "sensor_type":0, "update_address":-1, "name":"T1"},
        {"board":2, "channel":1, "sensor_type":0, "update_address":-1, "name":"T2"},
        {"board":2, "channel":2, "sensor_type":0, "update_address":-1, "name":"T3"},
        {"board":2, "channel":3, "sensor_type":0, "update_address":-1, "name":"T4"}
    ]
}
```

| Attribute | Description  | Data type | Data Description |
| --------- | -------------| --------- | -----------------|
| odata.type | odata type | string | |
| odata.id | odata id | string | |
| TemperatureConfigs | temperature sensor configs | array | |
| board | board index | int | range: 0 - 3 |
| channel | channel index | int | range: 0 - 3 |
| sensor_type | sensor type | int | 0: PT100, 1: PT1000 |
| update_address | update address | int | -1: not update |
| name | config name | string | |

## Write example

!!! example "Config RTD channel temperature sensors"
    Board index: 2, channel 0, sensor: PT100, update address: -1, name: T1 <br>
    Board index: 2, channel 1, sensor: PT1000, update address: -1, name: T2 <br>
    Board index: 2, channel 2, sensor: PT100, update address: -1, name: T3 <br>
    Board index: 2, channel 3, sensor: PT1000, update address: -1, name: T4 <br>

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
    "TemperatureConfigs":[
        {"board":2, "channel":0, "sensor_type":0, "update_address":-1, "name":"T1"},
        {"board":2, "channel":1, "sensor_type":1, "update_address":-1, "name":"T2"},
        {"board":2, "channel":2, "sensor_type":0, "update_address":-1, "name":"T3"},
        {"board":2, "channel":3, "sensor_type":1, "update_address":-1, "name":"T4"}
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

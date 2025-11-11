# Tutorial: OEM Kenmec Config Read/Write API for analog output voltage configs

## Overview

This API is used to read/write analog output voltage sensors configs, after analog output voltage sensors configs is set, the analog output voltage value will covert by sensor type.

!!! info "Example"
    Output: 0 % ~ 100 % mapping to 0mV ~ 10000mV <br>

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
    "AnalogOutputVoltageConfigs": [
        {"board":0, "channel":0, "sensor_type":0, "name":"C1"},
        {"board":0, "channel":1, "sensor_type":0, "name":"C2"},
    ]
}
```

| Attribute | Description  | Data type | Data Description |
| --------- | -------------| --------- | -----------------|
| odata.type | odata type | string | |
| odata.id | odata id | string | |
| AnalogOutputVoltageConfigs | analog output voltage sensor configs | array | |
| board | board index | int | range: 0 - 3 |
| channel | channel index | int | range: 0 - 3 |
| sensor_type | sensor type | int | 0: 0-100% convert to 0mV to 10000mV |
| name | config name | string | |

## Config write

!!! example "Config analog output voltage"
    Board index: 0, AO_2, sensor_type: 0, name: V1 <br>
    Board index: 0, AO_3, sensor_type: 0, name: V2 <br>

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
    "AnalogOutputVoltageConfigs":[
        {"board":0, "channel":2, "sensor_type":0, "name":"V1"},
        {"board":0, "channel":3, "sensor_type":0, "name":"V2"}
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

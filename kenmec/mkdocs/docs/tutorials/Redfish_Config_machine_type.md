# Tutorial: OEM Kenmec Config Read/Write API for machine type

## Overview

This API is used to read/write machine type.

## Read example

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
    "SystemConfigs": {
        "machine_type": "LS80"
    }
}
```

| Attribute | Description  | Data type | Data Description |
| --------- | -------------| --------- | -----------------|
| odata.type | odata type | string | |
| odata.id | odata id | string | |
| SystemConfigs | system configs | json object | |
| machine_type | machine type | string | currently only support: <br>"LS80" <br> "LX1400" |

## Write example

- Set machine type to LX1400

!!! warning "Set machine type"
    After set machine type, the control logic will be updated to related machine's control logic, so you need to make sure the hardware is compatible with the new machine type.

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
    "SystemConfigs": {
        "machine_type":"LX1400"
    }
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
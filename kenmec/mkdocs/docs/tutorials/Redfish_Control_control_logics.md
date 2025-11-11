# Tutorial: OEM Kenmec Control Logics Read/Write API

## Get all control logics information

### Endpoint

```bash
GET /redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics
```

### Request example

```curl
curl --location '192.168.124.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics'
```

### Response example

```json
{
    "@odata.type": "#ControlLogicCollection.v1_0_0.ControlLogicCollection",
    "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics",
    "Name": "Kenmec ControlLogics Collection",
    "Description": "Collection of ControlLogics for CDU 1",
    "Members@odata.count": 7,
    "Members": [
        {
            "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1"
        },
        {
            "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/2"
        },
        {
            "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/3"
        },
        {
            "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/4"
        },
        {
            "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/5"
        },
        {
            "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/6"
        },
        {
            "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/7"
        }
    ]
}
```

!!! info Member
    Members is an array of Control Logic information.

## Get control logic 1 information

### Endpoint

```bash
GET /redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1
```

### Request example

```curl
curl --location '192.168.124.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1'
```

### Response example

```json
{
    "@odata.type": "#ControlLogic.v1_0_0.ControlLogic",
    "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1",
    "Id": "1",
    "Name": "Control Logic 1",
    "Status": {
        "State": "Enabled",
        "Health": "OK"
    },
    "Actions": {
        "Oem": {
            "#ControlLogic.Read": {
                "target": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Read",
                "title": "Read Control Logic"
            },
            "#ControlLogic.Write": {
                "target": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Write",
                "title": "Write Control Logic"
            }
        }
    }
}
```

## Read control logic 1 all register data

### Endpoint

```bash
GET /redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Read
```

### Request example

```curl
curl --location '192.168.124.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Read'
``` 

### Response example

```json
{
    "@odata.type": "#KenmecControlLogic.v1_0_0.ReadResponse",
    "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Read",
    "CONTROL_LOGIC_1_ENABLE": 0,
    "F2": 0,
    "P4": 0,
    "P13": 0,
    "TARGET_TEMP": 120,
    "FLOW_SETPOINT": 200,
    "PUMP1_SPEED": 0,
    "PUMP2_SPEED": 0,
    "TEMP_CONTROL_MODE": 0,
    "PUMP1_MANUAL_MODE": 0,
    "PUMP2_MANUAL_MODE": 0,
    "VALVE_MANUAL_MODE": 0,
    "T4": 265,
    "T2": 258,
    "PUMP1_CONTROL": 0,
    "PUMP2_CONTROL": 1,
    "VALVE_SETPOINT": 0,
    "RegisterConfigs": [
        {"name": "CONTROL_LOGIC_1_ENABLE", "address": 41001, "address_default": 41001, "type": 2},
        {"name": "F2", "address": 42063, "address_default": 42063, "type": 2},
        {"name": "P4", "address": 42085, "address_default": 42085, "type": 2},
        {"name": "P13", "address": 42094, "address_default": 42094, "type": 2},
        {"name": "TARGET_TEMP", "address": 45001, "address_default": 45001, "type": 2},
        {"name": "FLOW_SETPOINT", "address": 45003, "address_default": 45003, "type": 2},
        {"name": "PUMP1_SPEED", "address": 45015, "address_default": 45015, "type": 2},
        {"name": "PUMP2_SPEED", "address": 45016, "address_default": 45016, "type": 2},
        {"name": "TEMP_CONTROL_MODE", "address": 45020, "address_default": 45020, "type": 2},
        {"name": "PUMP1_MANUAL_MODE", "address": 45021, "address_default": 45021, "type": 2},
        {"name": "PUMP2_MANUAL_MODE", "address": 45022, "address_default": 45022, "type": 2},
        {"name": "VALVE_MANUAL_MODE", "address": 45061, "address_default": 45061, "type": 2},
        {"name": "T4", "address": 413560, "address_default": 413560, "type": 2},
        {"name": "T2", "address": 413556, "address_default": 413556, "type": 2},
        {"name": "PUMP1_CONTROL", "address": 413512, "address_default": 413512, "type": 2},
        {"name": "PUMP2_CONTROL", "address": 413508, "address_default": 413508, "type": 2},
        {"name": "VALVE_SETPOINT", "address": 413504, "address_default": 413504, "type": 2},
    ]
}
```

| Attribute | Description  | Data type | Data Description |
| --------- | -------------| --------- | -----------------|
| odata.type | odata type | string | |
| odata.id | odata id | string | |
| "F2" and others predefined key | register data | | |
| RegisterConfigs | register configs | array | |
| name | register name | string | |
| address | register address | int | |
| address_default | register address default | int | |
| type | register type | int | 0: read only <br> 1: write only <br> 2: read/write |
| name | register name | string | |

!!! info "Type"
    If type is 0, the register is read only and not support write request.

## Write control logic 1 register data

!!! info "Example"
    "CONTROL_LOGIC_1_ENABLE" set to 1 <br>
    "FLOW_SETPOINT" set to 250 <br>

### Endpoint

```bash
POST /redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Write
```

### Request example

```curl
curl --location '192.168.124.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Write' \
--header 'Content-Type: application/json' \
--data '{
    "CONTROL_LOGIC_1_ENABLE": 1,
    "FLOW_SETPOINT": 250
}'
```

### Response example

```json
{
    "@odata.type": "#KenmecIOBoard.v1_0_0.WriteResponse",
    "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Write",
    "ControlLogicIdx": 1,
    "Status": "Success"
}
```

## Reconfigure control logic 1 register configs

!!! info "Reconfigure control logic register configs"
    Control logic registers have default address, you can modify the register address, so that you can change the input/output I/O channel by specific purpose.

!!! info "Example"
    "T4" change RTD channel from board 2, channel 3 to board 2, channel 4 <br>
    "T2" change RTD channel from board 2, channel 1 to board 2, channel 2 <br>

### Endpoint

```bash
POST /redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Write
```

### Request example

```curl
curl --location '192.168.0.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Write' \
--header 'Content-Type: application/json' \
--data '{
    "RegisterConfigs": [
        {"name": "T4", "address": 413562},
        {"name": "T2", "address": 413558}
    ]
}'
```

### Response example

```json
{
    "@odata.type": "#KenmecIOBoard.v1_0_0.WriteResponse",
    "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Write",
    "ControlLogicIdx": 1,
    "Status": "Success"
}
```
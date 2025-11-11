# Redfish API: OEM Kenmec Control Logics Read/Write API

## Board information

| USB slot | Board type |
|---------|-------------|
| 0 | AIO board |
| 1 | AIO board |
| 2 | RTD/PWM board |

## Get Hub's IP Address (via mDNS tool or Avahi Discovery tool)

!!! warning "mDNS tool"
    Must be in same network with Hub.

![Avahi Discovery](avahi_discovery.png)

---

# API List

## Get all control logics information

### Endpoint

```bash
/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics
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

!!! info "Member"
    Members is an array of Control Logic information.

## Get control logic 1 information

### Endpoint

```bash
/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1
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
/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Read
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
    "T11": 0,
    "T12": 0,
    "T17": 0,
    "T18": 0,
    "F2": 2,
    "P12": 12,
    "P13": 13,
    "TARGET_TEMP": 120,
    "FLOW_SETPOINT": 200,
    "TEMP_CONTROL_MODE": 0,
    "PUMP1_MANUAL_MODE": 0,
    "PUMP2_MANUAL_MODE": 0,
    "PUMP3_MANUAL_MODE": 0,
    "PUMP1_CONTROL": 0,
    "PUMP2_CONTROL": 0,
    "PUMP3_CONTROL": 0,
    "VALVE_MANUAL_MODE": 0
}
```

## Write control logic 1 register data

!!! info "Control logic 1 writeable attributes"
    "CONTROL_LOGIC_1_ENABLE"
    "TARGET_TEMP"
    "FLOW_SETPOINT"
    "TEMP_CONTROL_MODE"
    "PUMP1_MANUAL_MODE"
    "PUMP2_MANUAL_MODE"
    "PUMP3_MANUAL_MODE"
    "PUMP1_CONTROL"
    "PUMP2_CONTROL"
    "PUMP3_CONTROL"
    "PUMP1_CONTROL"
    "PUMP2_CONTROL"
    "PUMP3_CONTROL"
    "VALVE_OPENING"
    "VALVE_SETPOINT"
    "VALVE_MANUAL_MODE"

### Endpoint

```bash
/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics/1/Actions/Oem/ControlLogic.Write
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
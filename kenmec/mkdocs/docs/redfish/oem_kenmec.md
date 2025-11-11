# Redfish API: /Oem/Kenmec

## Overview

This document describes the Redfish API for Oem kenmec, it is used to get the Oem kenmec information.

## Endpoints

```bash
GET /redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec
```

## Request example

```bash
curl --location '192.168.0.184:8080/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec'
```

## Response example

```json
{
    "@odata.type": "#KenmecCDUOem.v1_0_0.KenmecCDUOem",
    "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec",
    "Name": "Kenmec OEM Extensions",
    "Description": "Kenmec-specific extensions for CDU resource",
    "IOBoards": {
        "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/IOBoards"
    },
    "Config": {
        "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config"
    },
    "ControlLogics": {
        "@odata.id": "/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics"
    }
}
```

| Attribute | Description | Data type | Data Description |
| --------- | ----------- | --------- | -----------------|
| @odata.type | odata type | string | |
| @odata.id | odata id | string | |
| Name | Name | string | |
| Description | Description | string | |
| IOBoards | IOBoards | object | kenmec IOBoards resource |
| Config | Config | object | kenmec config resource |
| ControlLogics | ControlLogics | object | kenmec control logis resource |

# Hub

![hub](./../img/hub.png)

---

## Specification

| Specific | description  |
| -------- | -----------  |
| Power | RATING: 5V 1A | 
| Interface | Ethernet <br> RS485 <br> USB host |

---

## Key Features

- Redfish API server
- Modbus RTU slave
- Integrate up to 4 sub-boards (IO/RTD board)
- Build-in control logic

---

## Data Flow

```mermaid
graph LR
    subgraph Hub[Hub]
        Control_logic
        Redfish_server
        Control_logic <--> Data_handler
        Redfish_server <--> Data_handler
        Modbus_slave <--> Data_handler
        Data_handler <--> USB_Host
        USB_Host
    end
    subgraph IO_board[IO_board]
        IO_USB_device
        Digital_output
        Digital_input
        Analog_IO
    end
    subgraph RTD board[RTD_board]
        RTD_USB_device
        RTD_input
        PWM_output
    end
    subgraph HID[HID]
        UI <--> Modbus_master
        Modbus_master
    end
    subgraph Redfish[Redfish]
        Redfish_client
    end
    Modbus_master <-->|Modbus rtu|Modbus_slave
    Redfish_client <-->|HTTP/HTTPS|Redfish_server
    USB_Host <-->|USB I/F|RTD_USB_device
    USB_Host <-->|USB I/F|IO_USB_device
```

---

## Get started

!!! abstract "Links"
    [Development environment](../getstarted/environment.md)<br> 
    [Development tools](../getstarted/setup.md)<br>
    [How to build SDK](../getstarted/build_sdk.md)<br>
    [How to build application](../getstarted/build_kenmec.md)<br>
    [How to build OTA image](../getstarted/build_OTA_image.md)<br>
    [Get Hub's IP address](../getstarted/get_hub_ip.md)<br>

---

## Redfish APIs (WIP)

!!! abstract "Links"
    [/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec](../redfish/oem_kenmec.md)<br>
    [/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/Config](../redfish/oem_kenmec_config.md)<br>
    [/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/IOBoards](../redfish/oem_kenmec_ioboards.md)<br>
    [/redfish/v1/ThermalEquipment/CDUs/1/Oem/Kenmec/ControlLogics](../redfish/oem_kenmec_control_logic.md)<br>

---

## Tutorials

!!! abstract "Links"
    [Add new machine to control logic](../tutorials/add_new_machine_to_control_logic.md)<br>
    [API: Config machine type](../tutorials/Redfish_Config_machine_type.md)<br>
    [API: Config Modbus devices](../tutorials/Redfish_Config_modbus_devices.md)<br>
    [API: Config Temperature sensors](../tutorials/Redfish_Config_temperature_sensors.md)<br>
    [API: Config Analog input current sensors](../tutorials/Redfish_Config_analog_input_current.md)<br>
    [API: Config Analog input voltage sensors](../tutorials/Redfish_Config_analog_input_voltage.md)<br>
    [API: Config Analog output current sensors](../tutorials/Redfish_Config_analog_output_current.md)<br>
    [API: Config Analog output voltage sensors](../tutorials/Redfish_Config_analog_output_voltage.md)<br>
    [API: Control the I/O board](../tutorials/Redfish_Control_IOBoards.md)<br>
    [API: Control the control logic](../tutorials/Redfish_Control_control_logics.md)<br>

---

## Firmware Version History

!!! abstract "v1.0.1.16"

    Date: 2025-09-30

    **Changes**

    + xxx

    **Note**

    + xxx
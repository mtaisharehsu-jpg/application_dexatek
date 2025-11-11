# RTD Board RTU Master

A Python-based GUI tool for PWM control and sensor monitoring on the RTD board via Modbus RTU communication.

## Features

- PWM frequency and duty control
- PWM frequency/width readback
- AD7124 resistance measurement (Ohm)
- Pressure sensor reading over RS485 sub-slave

## Prerequisites

### Hardware Requirements

- RS485 USB dongle/converter for serial communication with the device

### Software Requirements

The following Python packages are required:

```bash
sudo usermod -aG dialout $USER
sudo apt install python3-tk
pip3 install pymodbus pyserial
```

## Usage

### Starting the Application

Run the script using Python:

```bash
python3 rtd_board_rtu_master.py
```

### Connection Setup

1. Connect your RS485 USB dongle to your computer
2. Select the appropriate COM port from the dropdown menu (this will be the port assigned to your RS485 dongle)
3. Click "Modbus connect" to establish connection
   - Green button indicates successful connection
   - Red button indicates no connection
4. Use "refresh COM port" to update the available ports list

### Device Configuration

- Set the Slave ID (default: 100)
- Select the Board Slot (`0`–`3`)
- Select the Channel Number (`0`–`7`) where applicable
- For pressure sensor read, select the 485 Slave ID (`1`–`32`)

### Operations

All base addresses start at 11000 and are offset by (slot × 100).

#### PWM Frequency Set

- Action: Enter frequency value and click "PWM Frequency Set"
- Writes two registers

#### PWM Duty Set

- Action: Enter duty value (0–100%) and click "PWM Duty Set"

#### PWM Duty Get

- Action: Click "PWM Duty Get" to read (units: 0.1%)

#### PWM Frequency Get

- Action: Click "PWM Frequency Get" to read (units: microseconds)
- Reads two registers

#### PWM Width Get

- Action: Click "PWM Width Get" to read (units: microseconds)
- Reads two registers

#### AD7124 Resistance Get

- Action: Click "AD7124 Resistance Get" to read (units: Ohm)
- Reads two registers

#### Pressure Sensor Get (RS485)

- Set 485 Slave ID (1–32)
- Action: Click "Pressure sensor Get (485)" to read

### Results Display

The results of all operations (read/write) are displayed in the "Result" text area at the bottom of the window.

## Error Messages

- "connect error": Failed to establish Modbus connection
- "read error": Failed to read from the device
- "write error": Failed to write to the device
- "except error": Unexpected error occurred during operation

## System Compatibility

The application automatically detects and displays the operating system type:

- Windows
- Ubuntu/Linux

## Notes

- Ensure proper COM port permissions on Linux systems
- Verify the slave ID, slot, channel, and 485 slave ID match your device configuration
- The connection button color indicates the connection status (red = disconnected, green = connected)
- All configuration values should be within the valid range for your device

# Control Logic RTU Master

A Python-based GUI tool for monitoring and configuring control logic parameters via Modbus RTU communication.

## Features

- Control logic status monitoring
- Configuration of control logic parameters

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
python3 control_logic_rtu_master.py
```

### Connection Setup

1. Connect your RS485 USB dongle to your computer
2. Select the appropriate COM port from the dropdown menu (this will be the port assigned to your RS485 dongle)
3. Click "Modbus connect" to establish connection
   - Green button indicates successful connection
   - Red button indicates no connection
4. Use "refresh COM port" to update the available ports list

### Device Configuration

Enter the Slave ID (default: 100) in the provided field.

### Monitoring Control Logic Status

Select one of the following status parameters to monitor:

- `alarm`: System alarm status
- `t2_avg`: Average temperature for T2
- `t3_avg`: Average temperature for T3
- `t4_avg`: Average temperature for T4

Click "Control logic status get" to read the current value.

### Control Logic Configuration

#### Available Configuration Parameters

1. **Control Logic 1 Enable**
   - Purpose: Enable/disable control logic 1

2. **Fan Rotation Configuration**
   - Purpose: Configure fan rotation settings

3. **Target Coolant Temperature**
   - Purpose: Set target temperature for coolant

4. **Tolerance**
   - Purpose: Set temperature tolerance range

5. **Critical High**
   - Purpose: Set high temperature threshold

6. **Critical Low**
   - Purpose: Set low temperature threshold

7. **Moving Average Window Size**
   - Purpose: Configure the window size for moving average calculation

#### Reading Configuration Values

1. Select the desired parameter from the "Control logic config" dropdown
2. Click "Control logic config get" to read the current value

#### Setting Configuration Values

1. Select the parameter to modify from the dropdown
2. Enter the new value in the "Control logic config set value" field
3. Click "Control logic config set" to apply the change

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
- Verify the slave ID matches your device configuration
- The connection button color indicates the connection status (red = disconnected, green = connected)
- All configuration values should be within the valid range for your device
# I/O Board RTU Master

A Python-based GUI tool for controlling and monitoring the I/O board via Modbus RTU communication.

## Features

- Digital I/O (DO/DI)
- Analog I/O (AO/AI)
- Analog mode configuration per port (A/B/C/D)

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
python3 io_board_rtu_master.py
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

### Operations

#### Digital Output (DO)

- Action: Enter value in "Digital output value" and click "Digital Output"
- Value: 0 or 1

#### Digital Input (DI)

- Action: Click "Digital Input" to read

#### Analog Mode Set (per port A/B/C/D)

- Modes:
  - `AO_Voltage` → 0
  - `AO_Current` → 1
  - `AI_Voltage` → 2
  - `AI_Current_loop` → 4
  - `AI_Current_ext` → 5
- Action: Select port and mode, then click "Analog Set Mode"

#### Analog Output (AO)

- Voltage write:
  - Action: Enter value and click "Analog Output Voltage"

- Current write:
  - Action: Enter value and click "Analog Output Current"

#### Analog Input (AI)

- Voltage read:
  - Action: Click "Analog Input Voltage" to read
- Current read:
  - Action: Click "Analog Input Current" to read

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
- Verify the slave ID, slot, and channel match your device configuration
- The connection button color indicates the connection status (red = disconnected, green = connected)
- All configuration values should be within the valid range for your device

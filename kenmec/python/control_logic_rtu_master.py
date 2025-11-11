import sys
import tkinter as tk
from tkinter import scrolledtext
from tkinter import ttk, messagebox
from pymodbus.client import ModbusSerialClient
import serial.tools.list_ports

MODBUS_ADDRESS_CONTROL_LOGIC_STATUS_ALARM = 2
MODBUS_ADDRESS_CONTROL_LOGIC_STATUS_T2_AVG = 11
MODBUS_ADDRESS_CONTROL_LOGIC_STATUS_T3_AVG = 14
MODBUS_ADDRESS_CONTROL_LOGIC_STATUS_T4_AVG = 17

MODBUS_ADDRESS_CONTROL_LOGIC_1_ENABLE = 15000
MODBUS_ADDRESS_CONTROL_LOGIC_1_FAN_ROTATION_CONFIG = 15001
MODBUS_ADDRESS_CONTROL_LOGIC_1_TARGET_COOLANT_TEMP = 15002
MODBUS_ADDRESS_CONTROL_LOGIC_1_TOLERANCE = 15003
MODBUS_ADDRESS_CONTROL_LOGIC_1_CRITICAL_HIGH = 15004
MODBUS_ADDRESS_CONTROL_LOGIC_1_CRITICAL_LOW = 15005
MODBUS_ADDRESS_CONTROL_LOGIC_1_MOVING_AVERAGE_WINDOWS_SIZE = 15006

modbus_client = None

# Interval for looping manual reads (milliseconds)
MANUAL_READ_LOOP_INTERVAL_MS = 50

# Job id for tk.after so we can cancel the loop
manual_read_after_id = None

def list_com_ports():
    """get available COM port list"""
    global connect_button
    ports = serial.tools.list_ports.comports()
    connect_button.configure(bg='red')    
    return [port.device for port in ports]

def connect_modbus():
    """connect to selected COM port"""
    global modbus_client, connect_button
    port = port_var.get()
    if not port:
        messagebox.showwarning("Warning", "Please select COM port!")
        return

    try:
        # 如果已經有舊連線，先關閉
        if modbus_client and modbus_client.connected:
            modbus_client.close()

        # 建立 Modbus RTU 連線
        selected_baudrate = int(baud_var.get())
        modbus_client = ModbusSerialClient(
            port=port,
            baudrate=selected_baudrate,
            parity='N',
            stopbits=1,
            bytesize=8,
            timeout=1
        )

        if modbus_client.connect():
            connect_button.configure(bg='green')
            messagebox.showinfo("Success", f"connect to {port}")
            modbus_client.close()
        else:
            connect_button.configure(bg='red')
            messagebox.showerror("Error", f"connect to {port} failed")
    except Exception as e:
        connect_button.configure(bg='red')
        messagebox.showerror("Error", f"connect to {port} failed: \n{e}")

def refresh_ports():
    """refresh COM port"""
    ports = list_com_ports()
    port_combo["values"] = ports
    if ports:
        port_combo.current(0)

def read_holding_registers(address, slave_id, count=1):
    global modbus_client
    if not modbus_client.connect():
        result_text.delete(1.0, tk.END)
        result_text.insert(1.0, "connect error")
        return
    try:
        result = modbus_client.read_holding_registers(address=address, count=count, slave=slave_id)
        # print(result)
        if result.isError():
            result_text.delete(1.0, tk.END)
            result_text.insert(1.0, "read error")
        else:
            value = result.registers[0]
            # Show all registers if count > 1
            if count > 1:
                all_values = [str(reg) for reg in result.registers]
                value_str = ", ".join(all_values)
                result_text.delete(1.0, tk.END)
                result_text.insert(1.0, f"[{value_str}]")
                return
            result_text.delete(1.0, tk.END)
            result_text.insert(1.0, str(value))
    except Exception as e:
        result_text.delete(1.0, tk.END)
        result_text.insert(1.0, "except error")

def write_holding_register(address, slave_id, value=0):
    global modbus_client
    if not modbus_client.connect():
        result_text.delete(1.0, tk.END)
        result_text.insert(1.0, "connect error")
        return
    try:
        result = modbus_client.write_register(address=address, value=value, slave=slave_id)
        if result.isError():
            result_text.delete(1.0, tk.END)
            result_text.insert(1.0, "write error")
        else:
            result_text.delete(1.0, tk.END)
            result_text.insert(1.0, "write success")
    except Exception as e:
        result_text.delete(1.0, tk.END)
        result_text.insert(1.0, "except error")

def write_holding_registers(address, slave_id, value=0):
    global modbus_client
    if not modbus_client.connect():
        result_text.delete(1.0, tk.END)
        result_text.insert(1.0, "connect error")
        return
    try:
        # Convert single value to list of two identical values
        # Convert 4-byte value to two 16-bit registers
        low_word = value & 0xFFFF
        high_word = (value >> 16) & 0xFFFF
        values_list = [low_word, high_word]        
        result = modbus_client.write_registers(address=address, slave=slave_id, values=values_list)
        if result.isError():
            result_text.delete(1.0, tk.END)
            result_text.insert(1.0, "write error")
        else:
            result_text.delete(1.0, tk.END)
            result_text.insert(1.0, "write success")
    except Exception as e:
        result_text.delete(1.0, tk.END)
        result_text.insert(1.0, f"except error: {str(e)}")

def control_logic_status_button_cmd():
    slave_id = int(slave_id_entry.get())
    control_logic_address = control_logic_address_entry.get()
        
    if control_logic_address == "alarm":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_STATUS_ALARM
    elif control_logic_address == "t2_avg":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_STATUS_T2_AVG
    elif control_logic_address == "t3_avg":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_STATUS_T3_AVG
    elif control_logic_address == "t4_avg":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_STATUS_T4_AVG
    else:
        messagebox.showwarning("Warning", "Please select control logic address!")
        return
    
    read_holding_registers(address=control_logic_address, slave_id=slave_id, count=1)

def control_logic_config_get_button_cmd():
    slave_id = int(slave_id_entry.get())
    control_logic = control_logic_config_entry.get()
    
    if control_logic == "control_logic1_enable":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_ENABLE
    elif control_logic == "control_logic1_fan_rotation_config":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_FAN_ROTATION_CONFIG
    elif control_logic == "control_logic1_target_coolant_temp":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_TARGET_COOLANT_TEMP
    elif control_logic == "control_logic1_tolerance":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_TOLERANCE
    elif control_logic == "control_logic1_critical_high":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_CRITICAL_HIGH
    elif control_logic == "control_logic1_critical_low":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_CRITICAL_LOW
    elif control_logic == "control_logic1_moving_average_windows_size":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_MOVING_AVERAGE_WINDOWS_SIZE
    else:
        messagebox.showwarning("Warning", "Please select control logic address!")
        return
    
    read_holding_registers(address=control_logic_address, slave_id=slave_id, count=1)

def control_logic_config_set_button_cmd():
    slave_id = int(slave_id_entry.get())
    control_logic = control_logic_config_entry.get()
    control_logic_value = int(control_logic_config_value_entry.get())
    
    if control_logic == "control_logic1_enable":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_ENABLE
    elif control_logic == "control_logic1_fan_rotation_config":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_FAN_ROTATION_CONFIG
    elif control_logic == "control_logic1_target_coolant_temp":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_TARGET_COOLANT_TEMP
    elif control_logic == "control_logic1_tolerance":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_TOLERANCE
    elif control_logic == "control_logic1_critical_high":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_CRITICAL_HIGH
    elif control_logic == "control_logic1_critical_low":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_CRITICAL_LOW
    elif control_logic == "control_logic1_moving_average_windows_size":
        control_logic_address = MODBUS_ADDRESS_CONTROL_LOGIC_1_MOVING_AVERAGE_WINDOWS_SIZE
    else:
        messagebox.showwarning("Warning", "Please select control logic address!")
        return
    
    write_holding_register(address=control_logic_address, slave_id=slave_id, value=control_logic_value)

def manual_read_cmd():
	slave_id = int(slave_id_entry.get())
	try:
		address = int(manual_address_entry.get())
	except Exception:
		messagebox.showwarning("Warning", "Please input valid manual address!")
		return
	try:
		read_count = int(read_quantity_entry.get())
		if read_count <= 0:
			raise ValueError("read_count must be > 0")
	except Exception:
		messagebox.showwarning("Warning", "Please input valid read quantity!")
		return
	read_holding_registers(address=address, slave_id=slave_id, count=read_count)

def schedule_manual_read():
    """Schedule periodic manual reads if the loop checkbox is enabled."""
    global manual_read_after_id
    if manual_read_loop_var.get():
        manual_read_cmd()
        manual_read_after_id = window.after(MANUAL_READ_LOOP_INTERVAL_MS, schedule_manual_read)

def toggle_manual_read_loop():
    """Start or stop the manual read loop based on the checkbox state."""
    global manual_read_after_id
    if manual_read_loop_var.get():
        if manual_read_after_id is None:
            schedule_manual_read()
    else:
        if manual_read_after_id is not None:
            try:
                window.after_cancel(manual_read_after_id)
            except Exception:
                pass
            manual_read_after_id = None

# 判斷系統
if sys.platform.startswith('win'):
    os_type = "Windows"
elif sys.platform.startswith('linux'):
    os_type = "Ubuntu/Linux"
else:
    os_type = "Other system"

# UI
window = tk.Tk()
window.title("Control logic Modbus RTU Master")
window.geometry("800x800")

tk.Label(window, text=f"System: {os_type}", font=("Arial", 10)).pack(pady=5)

# Create frame for port selection and connect button
frame_port = tk.Frame(window)
frame_port.pack(pady=5)

port_var = tk.StringVar()
port_combo = ttk.Combobox(frame_port, textvariable=port_var, state="readonly")
port_combo.grid(row=0, column=0, padx=5)

# baudrate combobox
baud_var = tk.StringVar()
baud_combo = ttk.Combobox(frame_port, textvariable=baud_var, state="readonly", values=["9600", "19200", "38400", "115200"])
baud_combo.grid(row=0, column=1, padx=5)
baud_combo.current(3)

# connect button
connect_button = tk.Button(frame_port, text="Modbus connect", command=connect_modbus, bg='red')
connect_button.grid(row=0, column=2, padx=5)

# refresh button
tk.Button(frame_port, text="refresh COM port", command=refresh_ports).grid(row=0, column=3, padx=5)

# 啟動時先刷新一次
refresh_ports()

# Textbox slave id
frame_textbox_slave_id = tk.Frame(window)
frame_textbox_slave_id.pack()
tk.Label(frame_textbox_slave_id, text="Slave ID:").pack(side="left")
slave_id_entry = tk.Entry(frame_textbox_slave_id)
slave_id_entry.insert(0, "100")
slave_id_entry.pack(side="left")

# Combo control logic address
frame_combo_control_logic_address = tk.Frame(window)
frame_combo_control_logic_address.pack()
tk.Label(frame_combo_control_logic_address, text="Control logic status:").grid(row=0, column=0)
control_logic_address_entry = ttk.Combobox(frame_combo_control_logic_address, values=["alarm", "t2_avg", "t3_avg", "t4_avg"])
control_logic_address_entry.grid(row=0, column=1)
control_logic_address_entry.insert(0, "alarm")

# button control logic status
btn_control_logic_status = tk.Button(window, text="Control logic status get", command=control_logic_status_button_cmd)
btn_control_logic_status.pack(pady=5)

# Combo control logic config address
frame_combo_control_logic_config_address = tk.Frame(window)
frame_combo_control_logic_config_address.pack()
tk.Label(frame_combo_control_logic_config_address, text="Control logic config:").grid(row=0, column=0)
control_logic_config_entry = ttk.Combobox(frame_combo_control_logic_config_address, values=["control_logic1_enable", "control_logic1_fan_rotation_config", "control_logic1_target_coolant_temp", "control_logic1_tolerance", "control_logic1_critical_high", "control_logic1_critical_low", "control_logic1_moving_average_windows_size"])
control_logic_config_entry.grid(row=0, column=1)
control_logic_config_entry.configure(width=40)
control_logic_config_entry.insert(0, "control_logic1_enable")

# button control logic config
btn_control_logic_config = tk.Button(window, text="Control logic config get", command=control_logic_config_get_button_cmd)
btn_control_logic_config.pack(pady=5)


# textbox control logic config value
frame_textbox_control_logic_config_value = tk.Frame(window)
frame_textbox_control_logic_config_value.pack()
tk.Label(frame_textbox_control_logic_config_value, text="Control logic config set value:").grid(row=0, column=0)
control_logic_config_value_entry = tk.Entry(frame_textbox_control_logic_config_value)
control_logic_config_value_entry.insert(0, "0")
control_logic_config_value_entry.grid(row=0, column=1)

# button control logic config
btn_control_logic_config = tk.Button(window, text="Control logic config set", command=control_logic_config_set_button_cmd)
btn_control_logic_config.pack(pady=5)

# Manual address and commands
frame_manual_address = tk.Frame(window)
frame_manual_address.pack()
tk.Label(frame_manual_address, text="Start address:").pack(side="left")
manual_address_entry = tk.Entry(frame_manual_address)
manual_address_entry.insert(0, "15000")
manual_address_entry.pack(side="left")

# Textbox read quantity
frame_textbox_read_quantity = tk.Frame(window)
frame_textbox_read_quantity.pack()
tk.Label(frame_textbox_read_quantity, text="Quantity:").pack(side="left")
read_quantity_entry = tk.Entry(frame_textbox_read_quantity)
read_quantity_entry.insert(0, "100")
read_quantity_entry.pack(side="left")

btn_manual_read = tk.Button(window, text="Manual Read", command=manual_read_cmd)
btn_manual_read.pack(pady=5)

# Checkbox to loop manual read
manual_read_loop_var = tk.BooleanVar(value=False)
chk_manual_read_loop = tk.Checkbutton(window, text="Loop Manual Read", variable=manual_read_loop_var, command=toggle_manual_read_loop)
chk_manual_read_loop.pack(pady=2)

# result
frame_result = tk.Frame(window)
frame_result.pack()
tk.Label(frame_result, text="Result:").grid(row=0, column=0)
result_text = tk.Text(frame_result, height=10)
result_text.grid(row=0, column=1)

window.mainloop()
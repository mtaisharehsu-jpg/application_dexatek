import sys
import tkinter as tk
from tkinter import scrolledtext
from tkinter import ttk, messagebox
from pymodbus.client import ModbusSerialClient
import serial.tools.list_ports

MODBUS_ADDRESS_START = 11000

MODBUS_ADDRESS_DO = 1
MODBUS_ADDRESS_DI = 9

MODBUS_ADDRESS_ANALOG_MODE_PORT_A = 17
MODBUS_ADDRESS_ANALOG_MODE_PORT_B = 18
MODBUS_ADDRESS_ANALOG_MODE_PORT_C = 19
MODBUS_ADDRESS_ANALOG_MODE_PORT_D = 20

MODBUS_ADDRESS_AO_PORT_A_VALUE = 29

MODBUS_ADDRESS_AO_VOLTAGE = 37
MODBUS_ADDRESS_AO_CURRENT = 45

MODBUS_ADDRESS_AI_VOLTAGE = 53
MODBUS_ADDRESS_AI_CURRENT = 61

modbus_client = None

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
        if result.isError():
            result_text.delete(1.0, tk.END)
            result_text.insert(1.0, "read error")
        else:
            if count > 1:
                all_values = [str(reg) for reg in result.registers]
                value_str = ", ".join(all_values)
                result_text.delete(1.0, tk.END)
                result_text.insert(1.0, f"[{value_str}]")
                return
            value = result.registers[0]
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

def DO_button_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())
    number_id = int(number_id_entry.get())
    address = MODBUS_ADDRESS_START + (slot_id * 100) + MODBUS_ADDRESS_DO + number_id
    value = int(do_value_entry.get())
    write_holding_register(address=address, slave_id=slave_id, value=value)

def auto_do_loop():
    if auto_do_var.get():
        DO_button_cmd()
        # schedule next run in 500 ms
        window.after(500, auto_do_loop)

def toggle_auto_do():
    # start loop when enabled; stop naturally when disabled
    if auto_do_var.get():
        auto_do_loop()

def DI_button_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())
    number_id = int(number_id_entry.get())
    address = MODBUS_ADDRESS_START + (slot_id * 100) + MODBUS_ADDRESS_DI + number_id

    read_holding_registers(address=address, slave_id=slave_id, count=1)

def AO_VOLTAGE_button_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())

    analog_port = analog_port_entry.get()
    if analog_port == "A":
        shift = 0
    elif analog_port == "B":
        shift = 1
    elif analog_port == "C":
        shift = 2
    elif analog_port == "D":
        shift = 3

    value = int(ao_value_entry.get())
    address = MODBUS_ADDRESS_START + (slot_id * 100) + MODBUS_ADDRESS_AO_VOLTAGE + (shift*2)
    write_holding_registers(address=address, slave_id=slave_id, value=value)

def AO_CURRENT_button_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())

    analog_port = analog_port_entry.get()
    if analog_port == "A":
        shift = 0
    elif analog_port == "B":
        shift = 1
    elif analog_port == "C":
        shift = 2
    elif analog_port == "D":
        shift = 3

    value = int(ao_value_entry.get())
    address = MODBUS_ADDRESS_START + (slot_id * 100) + MODBUS_ADDRESS_AO_CURRENT + (shift*2)
    write_holding_registers(address=address, slave_id=slave_id, value=value)

def AI_VOLTAGE_button_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())
    analog_port = analog_port_entry.get()

    if analog_port == "A":
        shift = 0
    elif analog_port == "B":
        shift = 1
    elif analog_port == "C":
        shift = 2
    elif analog_port == "D":
        shift = 3

    address = MODBUS_ADDRESS_START + (slot_id * 100) + MODBUS_ADDRESS_AI_VOLTAGE + (shift*2)

    read_holding_registers(address=address, slave_id=slave_id, count=2)

def AI_CURRENT_button_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())
    analog_port = analog_port_entry.get()

    if analog_port == "A":
        shift = 0
    elif analog_port == "B":
        shift = 1
    elif analog_port == "C":
        shift = 2
    elif analog_port == "D":
        shift = 3

    address = MODBUS_ADDRESS_START + (slot_id * 100) + MODBUS_ADDRESS_AI_CURRENT + (shift*2)

    read_holding_registers(address=address, slave_id=slave_id, count=2)

def ANALOG_SET_MODE_button_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())
    analog_port = analog_port_entry.get()
    analog_mode = analog_mode_entry.get()
    
    if analog_port == "A":
        address = MODBUS_ADDRESS_START + (slot_id * 100) + MODBUS_ADDRESS_ANALOG_MODE_PORT_A
    elif analog_port == "B":
        address = MODBUS_ADDRESS_START + (slot_id * 100) + MODBUS_ADDRESS_ANALOG_MODE_PORT_B
    elif analog_port == "C":
        address = MODBUS_ADDRESS_START + (slot_id * 100) + MODBUS_ADDRESS_ANALOG_MODE_PORT_C
    elif analog_port == "D":
        address = MODBUS_ADDRESS_START + (slot_id * 100) + MODBUS_ADDRESS_ANALOG_MODE_PORT_D

    # Validate analog_mode range (0, 1, 2, 4, 5 based on UI combobox values)
    if analog_mode == "AO_Voltage":
        analog_mode = 0
    elif analog_mode == "AO_Current":
        analog_mode = 1
    elif analog_mode == "AI_Voltage":
        analog_mode = 2
    elif analog_mode == "AI_Current_loop":
        analog_mode = 3
    elif analog_mode == "AI_Current_ext":
        analog_mode = 4
    else:
        result_text.delete(1.0, tk.END)
        result_text.insert(1.0, "Invalid analog mode")
        return
    
    write_holding_register(address=address, slave_id=slave_id, value=analog_mode)

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

# 判斷系統
if sys.platform.startswith('win'):
    os_type = "Windows"
elif sys.platform.startswith('linux'):
    os_type = "Ubuntu/Linux"
else:
    os_type = "Other system"

# UI
window = tk.Tk()
window.title("I/O board Modbus RTU Master")
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

# Combo I/O slot
frame_combo_slot = tk.Frame(window)
frame_combo_slot.pack()
tk.Label(frame_combo_slot, text="Board Slot:").grid(row=0, column=0)
slot_id_entry = ttk.Combobox(frame_combo_slot, values=["0", "1", "2", "3"])
slot_id_entry.grid(row=0, column=1)
slot_id_entry.insert(0, "0")

# Combo I/O number
frame_combo_number = tk.Frame(window)
frame_combo_number.pack()
tk.Label(frame_combo_number, text="Channel Number:").grid(row=0, column=0)
number_id_entry = ttk.Combobox(frame_combo_number, values=["0", "1", "2", "3", "4", "5", "6", "7"])
number_id_entry.grid(row=0, column=1)
number_id_entry.insert(0, "0")

# textbox Digital Output Value
frame_textbox_do_value = tk.Frame(window)
frame_textbox_do_value.pack()
tk.Label(frame_textbox_do_value, text="Digital output value:").grid(row=0, column=0)
do_value_entry = tk.Entry(frame_textbox_do_value)
do_value_entry.insert(0, "0")
do_value_entry.grid(row=0, column=1)

# button DO
btn_do = tk.Button(window, text="Digital Output", command=DO_button_cmd)
btn_do.pack(pady=5)

# checkbox Auto DO
auto_do_var = tk.BooleanVar(value=False)
auto_do_checkbox = tk.Checkbutton(window, text="Auto Digital Output", variable=auto_do_var, command=toggle_auto_do)
auto_do_checkbox.pack(pady=5)

# button DI
btn_di = tk.Button(window, text="Digital Input", command=DI_button_cmd)
btn_di.pack(pady=5)

# Combo Analog port
frame_combo_analog_port = tk.Frame(window)
frame_combo_analog_port.pack()
tk.Label(frame_combo_analog_port, text="Analog port:").grid(row=0, column=0)
analog_port_entry = ttk.Combobox(frame_combo_analog_port, values=["A", "B", "C", "D"])
analog_port_entry.grid(row=0, column=1)
analog_port_entry.insert(0, "A")

# Combo Analog mode
frame_combo_analog_mode = tk.Frame(window)
frame_combo_analog_mode.pack()
tk.Label(frame_combo_analog_mode, text="Analog mode:").grid(row=0, column=0)
analog_mode_entry = ttk.Combobox(frame_combo_analog_mode, values=["AO_Voltage", "AO_Current", "AI_Voltage", "AI_Current_loop", "AI_Current_ext"])
analog_mode_entry.grid(row=0, column=1)
analog_mode_entry.current(0)

# button AO_SET_MODE
btn_analog_set_mode = tk.Button(window, text="Analog Set Mode", command=ANALOG_SET_MODE_button_cmd)
btn_analog_set_mode.pack(pady=5)

# textbox Analog Output Value
frame_textbox_ao_value = tk.Frame(window)
frame_textbox_ao_value.pack()
tk.Label(frame_textbox_ao_value, text="Analog output value:").grid(row=0, column=0)
ao_value_entry = tk.Entry(frame_textbox_ao_value)
ao_value_entry.insert(0, "0")
ao_value_entry.grid(row=0, column=1)

# button AO_VOLTAGE
btn_ao_voltage = tk.Button(window, text="Analog Output Voltage", command=AO_VOLTAGE_button_cmd)
btn_ao_voltage.pack(pady=5)

# button AO_CURRENT
btn_ao_current = tk.Button(window, text="Analog Output Current", command=AO_CURRENT_button_cmd)
btn_ao_current.pack(pady=5)

# button AI_VOLTAGE
btn_ai_voltage = tk.Button(window, text="Analog Input Voltage", command=AI_VOLTAGE_button_cmd)
btn_ai_voltage.pack(pady=5)

# button AI_CURRENT
btn_ai_current = tk.Button(window, text="Analog Input Current", command=AI_CURRENT_button_cmd)
btn_ai_current.pack(pady=5)

# Manual address and commands
frame_manual_address = tk.Frame(window)
frame_manual_address.pack()
tk.Label(frame_manual_address, text="Start address:").pack(side="left")
manual_address_entry = tk.Entry(frame_manual_address)
manual_address_entry.insert(0, "11001")
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

# result
frame_result = tk.Frame(window)
frame_result.pack()
tk.Label(frame_result, text="Result:").grid(row=0, column=0)
result_text = tk.Text(frame_result, height=10)
result_text.grid(row=0, column=1)

window.mainloop()
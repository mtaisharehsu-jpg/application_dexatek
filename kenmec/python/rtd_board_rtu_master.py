import sys
import tkinter as tk
from tkinter import scrolledtext
from tkinter import ttk, messagebox
from pymodbus.client import ModbusSerialClient
import serial.tools.list_ports

MODBUS_ADDRESS_START = 11000

MODBUS_ADDRESS_PWM_FREQUENCY = 504
MODBUS_ADDRESS_PWM_DUTY_SET = 506
MODBUS_ADDRESS_PWM_DUTY_GET = 514
MODBUS_ADDRESS_PWM_FREQUENCY_GET = 522
MODBUS_ADDRESS_PWM_WIDTH_GET = 538
MODBUS_ADDRESS_AD7124_RESISTANCE_GET = 554
MODBUS_ADDRESS_PRESSURE = 578

modbus_client = None
connect_button = None

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

def PWM_frequency_set_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())

    value = int(frequency_value_entry.get())
    address = MODBUS_ADDRESS_START + (slot_id * 1000) + MODBUS_ADDRESS_PWM_FREQUENCY
    write_holding_registers(address=address, slave_id=slave_id, value=value)

def PWM_duty_set_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())
    number_id = int(number_id_entry.get())

    value = int(duty_value_entry.get())
    address = MODBUS_ADDRESS_START + (slot_id * 1000) + MODBUS_ADDRESS_PWM_DUTY_SET + number_id
    write_holding_register(address=address, slave_id=slave_id, value=value)

def PWM_duty_get_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())
    number_id = int(number_id_entry.get())

    address = MODBUS_ADDRESS_START + (slot_id * 1000) + MODBUS_ADDRESS_PWM_DUTY_GET + number_id

    read_holding_registers(address=address, slave_id=slave_id, count=1)

def PWM_frequency_get_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())
    number_id = int(number_id_entry.get())

    address = MODBUS_ADDRESS_START + (slot_id * 1000) + MODBUS_ADDRESS_PWM_FREQUENCY_GET + number_id *2

    read_holding_registers(address=address, slave_id=slave_id, count=2)

def PWM_width_get_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())
    number_id = int(number_id_entry.get())

    address = MODBUS_ADDRESS_START + (slot_id * 1000) + MODBUS_ADDRESS_PWM_WIDTH_GET + number_id *2

    read_holding_registers(address=address, slave_id=slave_id, count=2)

def AD7124_resistance_get_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())
    number_id = int(number_id_entry.get())

    address = MODBUS_ADDRESS_START + (slot_id * 1000) + MODBUS_ADDRESS_AD7124_RESISTANCE_GET + number_id *2
    
    read_holding_registers(address=address, slave_id=slave_id, count=2)

def pressure_sensor_get_cmd():
    slave_id = int(slave_id_entry.get())
    slot_id = int(slot_id_entry.get())
    modbus_slave_id = int(modbus_slave_id_entry.get())
    # shift
    modbus_slave_id -= 1
    address = MODBUS_ADDRESS_START + (slot_id * 1000) + MODBUS_ADDRESS_PRESSURE + modbus_slave_id

    read_holding_registers(address=address, slave_id=slave_id, count=1)

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
window.title("RTD board Modbus RTU Master")
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

# textbox frequency value
frame_textbox_frequency_value = tk.Frame(window)
frame_textbox_frequency_value.pack()
tk.Label(frame_textbox_frequency_value, text="Frequency value:").grid(row=0, column=0)
frequency_value_entry = tk.Entry(frame_textbox_frequency_value)
frequency_value_entry.insert(0, "0")
frequency_value_entry.grid(row=0, column=1)

# button PWM Set Mode
btn_pwm_frequency = tk.Button(window, text="PWM Frequency Set", command=PWM_frequency_set_cmd)
btn_pwm_frequency.pack(pady=5)

# Combo channel number
frame_combo_number = tk.Frame(window)
frame_combo_number.pack()
tk.Label(frame_combo_number, text="Channel Number:").grid(row=0, column=0)
number_id_entry = ttk.Combobox(frame_combo_number, values=["0", "1", "2", "3", "4", "5", "6", "7"])
number_id_entry.grid(row=0, column=1)
number_id_entry.insert(0, "0")

# textbox duty value
frame_textbox_duty_value = tk.Frame(window)
frame_textbox_duty_value.pack()
tk.Label(frame_textbox_duty_value, text="Duty value:").grid(row=0, column=0)
duty_value_entry = tk.Entry(frame_textbox_duty_value)
duty_value_entry.insert(0, "0")
duty_value_entry.grid(row=0, column=1)

# button PWM Duty set
btn_pwm_duty = tk.Button(window, text="PWM Duty Set (0-100%)", command=PWM_duty_set_cmd)
btn_pwm_duty.pack(pady=5)

# button PWM Duty ge
btn_pwm_duty_get = tk.Button(window, text="PWM Duty Get (0.1%)", command=PWM_duty_get_cmd)
btn_pwm_duty_get.pack(pady=5)

# button PWM frequency get
btn_pwm_frequency_get = tk.Button(window, text="PWM Frequency Get (us)", command=PWM_frequency_get_cmd)
btn_pwm_frequency_get.pack(pady=5)

# button PWM width get
btn_pwm_width_get = tk.Button(window, text="PWM Width Get (us)", command=PWM_width_get_cmd)
btn_pwm_width_get.pack(pady=5)

# button AD7124 Resistance get
btn_ad7124_resistance_get = tk.Button(window, text="AD7124 Resistance Get (Ohm)", command=AD7124_resistance_get_cmd)
btn_ad7124_resistance_get.pack(pady=5)

# Combo 485 slave id
frame_combo_485_slave_id = tk.Frame(window)
frame_combo_485_slave_id.pack()
tk.Label(frame_combo_485_slave_id, text="485 Slave ID:").grid(row=0, column=0)
modbus_slave_id_entry = ttk.Combobox(frame_combo_485_slave_id, values=["1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32"])
modbus_slave_id_entry.grid(row=0, column=1)
modbus_slave_id_entry.insert(0, "1")

# button Pressure sensor get
btn_pressure_sensor_get = tk.Button(window, text="Pressure sensor Get (485)", command=pressure_sensor_get_cmd)
btn_pressure_sensor_get.pack(pady=5)

# button AI_VOLTAGE
# btn_ai_voltage = tk.Button(window, text="Capture Duty", command=AI_VOLTAGE_button_cmd)
# btn_ai_voltage.pack(pady=5)

# Manual address and commands
frame_manual_address = tk.Frame(window)
frame_manual_address.pack()
tk.Label(frame_manual_address, text="Start address:").pack(side="left")
manual_address_entry = tk.Entry(frame_manual_address)
manual_address_entry.insert(0, "12500")
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
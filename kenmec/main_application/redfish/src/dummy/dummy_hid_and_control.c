#include <stdint.h>
#include <stddef.h>

// Provide minimal dummy implementations to satisfy linker

// HID manager
void hid_manager_port_pid_get(uint16_t port_index, uint16_t *out_pid)
{
    (void)port_index;
    if (out_pid) {
        *out_pid = 0; // Unknown PID by default
    }
}

// Control hardware write APIs
int control_hardware_pwm_duty_set(uint16_t port_index, uint16_t channel, int value)
{
    (void)port_index; (void)channel; (void)value;
    return 0; // SUCCESS
}

int control_hardware_pwm_freq_set(uint16_t port_index, int value)
{
    (void)port_index; (void)value;
    return 0; // SUCCESS
}

int control_hardware_digital_output_set(uint16_t port_index, uint16_t channel, int value)
{
    (void)port_index; (void)channel; (void)value;
    return 0; // SUCCESS
}

int control_hardware_AI_AO_mode_set(uint16_t port_index, uint16_t channel, int value)
{
    (void)port_index; (void)channel; (void)value;
    return 0; // SUCCESS
}

int control_hardware_analog_output_voltage_set(uint16_t port_index, uint16_t channel, int value)
{
    (void)port_index; (void)channel; (void)value;
    return 0; // SUCCESS
}

int control_hardware_analog_output_current_set(uint16_t port_index, uint16_t channel, int value)
{
    (void)port_index; (void)channel; (void)value;
    return 0; // SUCCESS
}

// Control hardware read-from-RAM APIs
void control_hardware_pwm_duty_all_get_from_ram(uint16_t port_index, uint16_t out_values[8])
{
    (void)port_index;
    if (!out_values) return;
    for (int i = 0; i < 8; ++i) out_values[i] = 0;
}

void control_hardware_pwm_freq_all_get_from_ram(uint16_t port_index, uint32_t out_values[8])
{
    (void)port_index;
    if (!out_values) return;
    for (int i = 0; i < 8; ++i) out_values[i] = 0;
}

void control_hardware_pwm_period_all_get_from_ram(uint16_t port_index, uint32_t out_values[8])
{
    (void)port_index;
    if (!out_values) return;
    for (int i = 0; i < 8; ++i) out_values[i] = 0;
}

void control_hardware_resistor_all_get_from_ram(uint16_t port_index, uint32_t out_values[8])
{
    (void)port_index;
    if (!out_values) return;
    for (int i = 0; i < 8; ++i) out_values[i] = 0;
}

void control_hardware_digital_output_all_get_from_ram(uint16_t port_index, uint16_t out_values[8])
{
    (void)port_index;
    if (!out_values) return;
    for (int i = 0; i < 8; ++i) out_values[i] = 0;
}

void control_hardware_digital_input_all_get_from_ram(uint16_t port_index, uint16_t out_values[8])
{
    (void)port_index;
    if (!out_values) return;
    for (int i = 0; i < 8; ++i) out_values[i] = 0;
}

void control_hardware_analog_mode_all_get_from_ram(uint16_t port_index, uint16_t out_values[8])
{
    (void)port_index;
    if (!out_values) return;
    for (int i = 0; i < 8; ++i) out_values[i] = 0;
}

void control_hardware_analog_input_voltage_get_from_ram(uint16_t port_index, uint16_t channel, int32_t *out_value)
{
    (void)port_index; (void)channel;
    if (out_value) *out_value = 0;
}

void control_hardware_analog_input_current_get_from_ram(uint16_t port_index, uint16_t channel, int32_t *out_value)
{
    (void)port_index; (void)channel;
    if (out_value) *out_value = 0;
}



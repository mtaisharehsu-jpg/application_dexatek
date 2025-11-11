# Tutorial: Add new machine to control logic architecture

## Overview

This tutorial is used to add a new machine to the control logic architecture.

## 1. Copy the existing control logic source files from the existing machine

```bash
cd kenmec/main_application/control_logic
cp -r ./ls80 ./ls90
```

!!! info "Info"
    ls80 is the existing machine. <br>
    ls90 is the new machine

## 2. Add source files to Makefile

`Makefile` in `kenmec/main_application/Makefile`

```bash
...
SRCS += $(wildcard $(root)/control_logic/ls90/*.c)
...
```

## 3. Rename the new machine folder source files

```bash
cd ls90/
mv control_logic_ls80_1.c control_logic_ls90_1.c
...
mv control_logic_ls80.h control_logic_ls90.h
```

## 4. Modify the new machine folder source files

### 4.1 Rename function name and define

```c
control_logic_ls80_1_temperature_control() -> control_logic_ls90_1_temperature_control()
control_logic_ls80_1_temperature_control_init() -> control_logic_ls90_1_temperature_control_init()
control_logic_ls80_1_config_get() -> control_logic_ls90_1_config_get()
#ifndef CONTROL_LOGIC_LS80_H -> #ifndef CONTROL_LOGIC_LS90_H
#define CONTROL_LOGIC_LS80_H -> #define CONTROL_LOGIC_LS90_H
```

### 4.2 Define CONFIG_REGISTER_FILE_PATH

```c
#define CONFIG_REGISTER_FILE_PATH "control_logic_ls90.json"
```

### 4.3 Define CONFIG_REGISTER_LIST_SIZE

```c
#define CONFIG_REGISTER_LIST_SIZE 25
```

### 4.4 Revise source code based on the new machine

```c
...
...
...
```

## 5. Modify the control logic related source files

### 5.1 control_logic_common.h

- Add new machine type: CONTROL_LOGIC_MACHINE_TYPE_LS90.

```c
typedef enum {
    CONTROL_LOGIC_MACHINE_TYPE_LS80,
    CONTROL_LOGIC_MACHINE_TYPE_LX1400,
    CONTROL_LOGIC_MACHINE_TYPE_LS90,
    CONTROL_LOGIC_MACHINE_TYPE_DEFAULT = CONTROL_LOGIC_MACHINE_TYPE_LS80,
} control_logic_machine_type_t;
```

### 5.2 control_logic_common.c

- Add new machine type to control_logic_api_data_append_to_json()

```c
switch (machine_type) {
    ...
    case CONTROL_LOGIC_MACHINE_TYPE_LS90: {
        switch (logic_id) {
            case 1:
                control_logic_ls90_1_config_get(&list_size, &register_list, &file_path);
                ret = control_logic_data_append_to_json(json_root, register_list, list_size);
                break;
            case ...:
                ...
                break;
            default:
                break;
        }
        break;
    }
    ...
}
```

- Add new machine type to control_logic_api_write_by_json()

```c
switch (machine_type) {
    ...
    case CONTROL_LOGIC_MACHINE_TYPE_LS90: {
        switch (logic_id) {
            case 1:
                control_logic_ls90_1_config_get(&list_size, &register_list, &file_path);
                ret = control_logic_write_by_json(jsonPayload, timeout_ms, file_path, register_list, list_size);
                break;
            case ...:
                ...
                break;
            default:
                break;
        }
        break;
    }
    ...
}
```

### 5.3 control_logic_config.c

- Add new machine type: LS90

```c
control_logic_machine_type_t control_logic_config_get_machine_type(void)
{
    control_logic_machine_type_t machine_type = CONTROL_LOGIC_MACHINE_TYPE_DEFAULT;

    if (_system_configs != NULL) {
        if (strncmp(_system_configs->machine_type, "LS80", sizeof(_system_configs->machine_type)) == 0) {
            machine_type = CONTROL_LOGIC_MACHINE_TYPE_LS80;
        }
        else if (strncmp(_system_configs->machine_type, "LX1400", sizeof(_system_configs->machine_type)) == 0) {
            machine_type = CONTROL_LOGIC_MACHINE_TYPE_LX1400;
        }
        else if (strncmp(_system_configs->machine_type, "LS90", sizeof(_system_configs->machine_type)) == 0) {
            machine_type = CONTROL_LOGIC_MACHINE_TYPE_LS90;
        }
    }

    return machine_type;
}
```

### 5.4 control_logic_hardware.c

- Add new machine hardware initialization code to control_hardware_init()

```c
switch (machine_type) {
    ...
    case CONTROL_LOGIC_MACHINE_TYPE_LS90:
        ret |= control_hardware_AI_AO_mode_set(0, 0, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
        ret |= control_hardware_AI_AO_mode_set(0, 1, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
        ret |= control_hardware_AI_AO_mode_set(0, 2, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
        ret |= control_hardware_AI_AO_mode_set(0, 3, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
        ret |= control_hardware_AI_AO_mode_set(1, 0, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
        ret |= control_hardware_AI_AO_mode_set(1, 1, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
        ret |= control_hardware_AI_AO_mode_set(1, 2, AI_AO_MODE_CURRENT_IN_EXTERNAL, 2000);
        ret |= control_hardware_AI_AO_mode_set(1, 3, AI_AO_MODE_CURRENT_OUT, 2000);
        debug(tag, "control_hardware_AI_AO_mode_set ret = %d", ret);
        break;
    ...
}
```

### 5.5 control_logic_manager.c

- Add new machine control logic's application / initialization function pointer to control_logic_manager_set_function_pointer()

```c
switch (machine_type) {
    ...
    case CONTROL_LOGIC_MACHINE_TYPE_LS90:
        ret = SUCCESS;
        // control function
        CONTROL_LOGIC_ARRAY[0].func = control_logic_ls90_1_temperature_control;
        ...
        // init function
        CONTROL_LOGIC_ARRAY[0].init = control_logic_ls90_1_temperature_control_init;
        ...
        break;
    ...
}
```
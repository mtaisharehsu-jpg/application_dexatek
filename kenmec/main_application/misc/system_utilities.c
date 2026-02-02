#include "dexatek/main_application/include/application_common.h"

#include "kenmec/main_application/control_logic/control_logic_register.h"
#include "kenmec/main_application/control_logic/control_logic_config.h"

void system_reset(void)
{
    // Intentionally empty
    printf("************** system_reset **************\n");

    sleep(3);

    system("reboot");
}

void system_firmware_update(void)
{
    // Intentionally empty
    printf("************** system_firmware_update **************\n");
    system("md5sum /tmp/redfish_upload_fw.swu");

    system("sysupd -f /tmp/redfish_upload_fw.swu");

    system("sync");

    system_reset();
}

void system_firmware_file_path(char *path)
{
    // Intentionally empty
    printf("************** system_firmware_file_path: /tmp/redfish_upload_fw.swu **************\n");
    strcpy(path, "/tmp/redfish_upload_fw.swu");
}

void system_model_get(char *model)
{
    // Intentionally empty
    printf("************** system_model_get **************\n");

    const char *machine_type_name = control_logic_config_get_model_name();

    // check if machine type name is NULL
    if (machine_type_name == NULL) {
        strcpy(model, "UNKNOWN");
        return;
    }

    // copy machine type name to model
    strcpy(model, machine_type_name);
}

void system_serial_number_get(char *serial_number)
{
    // Intentionally empty
    printf("************** system_serial_number_get **************\n");
    const char *product_sn = control_logic_config_get_product_sn();
    if (product_sn == NULL) {
        strcpy(serial_number, "1234567890");
        return;
    }

    // copy product sn to serial number
    strcpy(serial_number, product_sn);
}

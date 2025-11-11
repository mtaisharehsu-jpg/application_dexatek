#include "dexatek/main_application/include/application_common.h"


void system_reset(void)
{
    // Intentionally empty
    printf("************** system_reset **************\n");
}

void system_firmware_update(void)
{
    // Intentionally empty
    printf("************** system_firmware_update **************\n");
}

void system_firmware_file_path(char *path)
{
    // Intentionally empty
    printf("************** system_firmware_file_path **************\n");
    strcpy(path, "./redfish_upload_fw.swu");
}
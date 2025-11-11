#include "dexatek/main_application/include/application_common.h"

#include <stdio.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------
   								Defined Constants
 ---------------------------------------------------------------------------*/

static const char *tag = "config_backup";

#define CONFIG_BACKUP_FILE_NAME "/tmp/config_backup.tar.gz"
#define CONFIG_BACKUP_FILES_LIST "/usrdata/*"

/*---------------------------------------------------------------------------
   								Variables
 ---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
   								Implementation
 ---------------------------------------------------------------------------*/

int config_backup_create(void)
{
    int ret = 0;
    char command[256];
    snprintf(command, sizeof(command), "tar czf %s %s", CONFIG_BACKUP_FILE_NAME, CONFIG_BACKUP_FILES_LIST);
    debug(tag, "command: %s", command);

    ret = system(command);
    if (ret != 0) {
        error(tag, "tar czf %s %s failed", CONFIG_BACKUP_FILE_NAME, CONFIG_BACKUP_FILES_LIST);
        ret = FAIL;
    }

    return ret;
}

int config_backup_restore(void)
{
    int ret = SUCCESS;
    char command[256];
    snprintf(command, sizeof(command), "tar xzf %s -C /", CONFIG_BACKUP_FILE_NAME);
    debug(tag, "command: %s", command);
    ret = system(command);

    if (ret != 0) {
        error(tag, "tar xzf %s -C / failed", CONFIG_BACKUP_FILE_NAME);
        ret = FAIL;
    }

    return ret;
}
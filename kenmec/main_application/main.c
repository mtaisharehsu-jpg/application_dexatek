#include "dexatek/main_application/include/application_common.h"

#include "dexatek/main_application/main_application.h"
#include "dexatek/main_application/include/utilities/platform_watchdog.h"
#include "dexatek/main_application/managers/ethernet_manager/ethernet_manager.h"
#include "dexatek/main_application/managers/modbus_manager/modbus_manager.h"
#include "dexatek/main_application/managers/hid_manager/hid_manager.h"
#include "dexatek/main_application/services/include/sntp_service.h"

#include "kenmec/main_application/kenmec_config.h"
#include "kenmec/main_application/control_logic/control_logic_manager.h"

#include "kenmec/main_application/redfish/include/redfish_init.h"

/*---------------------------------------------------------------------------
                            Defined Constants
 ---------------------------------------------------------------------------*/
static const char* tag = "kenmec_main";

/*---------------------------------------------------------------------------
                                Variables
 ---------------------------------------------------------------------------*/
static bool _thread_aborted = false;

/*---------------------------------------------------------------------------
                            Function Prototypes
 ---------------------------------------------------------------------------*/
static void _handle_sigint(int signo);
static void _main_exit(void);
void application_run(void);
int application_stop(void);

/*---------------------------------------------------------------------------
                                Implementation
 ---------------------------------------------------------------------------*/

static void _main_exit(void)
{
    debug(tag, "Application exiting...");
    application_stop();
}

static void _handle_sigint(int signo)
{
    if (signo == SIGINT) {
        debug(tag, "Caught SIGINT!");
        _main_exit();
    }
}

static void _main_application_process(void)
{
    debug(tag, "Starting main application process...");

    // Initialize watchdog service.
#if defined(CONFIG_APPLICATION_WATCHDOG_ENABLE) && CONFIG_APPLICATION_WATCHDOG_ENABLE
	platform_watchdog_start(CONFIG_APPLICATION_WATCHDOG_TIMEOUT_SECONDS);
#endif

    time_delay_ms(10000);

    // Initialize application components.   
    ethernet_manager_init();
    // sntp_service_init();
    
    hid_manager_init();

    modbus_manager_init();

    control_logic_config_init();
    control_logic_manager_init();
    control_logic_manager_start();
    control_logic_update_init();

    redfish_init();

    // Main application loop
    while (!_thread_aborted) {
#if defined(CONFIG_APPLICATION_WATCHDOG_ENABLE) && CONFIG_APPLICATION_WATCHDOG_ENABLE
		platform_watchdog_refresh();
#endif
        sleep(5);
    }
    
    debug(tag, "Main application process exiting");
}

void application_run(void)
{
    printf("\n");
    printf("*********************************************\n");
    printf("* KENMEC Main Application                    \n");
    printf("* Version: %d.%d.%d                          \n", 
        CONFIG_APPLICATION_MAJOR_VERSION, CONFIG_APPLICATION_MINOR_VERSION, CONFIG_APPLICATION_PATCH_VERSION);
    printf("* Build Number: %d                           \n", 
        CONFIG_APPLICATION_VERSION_CODE_NUMBER);
    printf("* Build Date: %s                             \n", __DATE__);
    printf("* Build Time: %s                             \n", __TIME__);
    printf("*********************************************\n");
    printf("\n");

    uint8_t major = 0;
    uint8_t minor = 0;
    uint8_t patch = 0;
    uint8_t version_code_number = 0;

    libdexatek_version_get(&major, &minor, &patch, &version_code_number);
    
    printf("\n");
    printf("*********************************************\n");
    printf("* libdexatek                                 \n");
    printf("* Version: %d.%d.%d                          \n", 
        major, minor, patch);
    printf("* Build Number: %d                           \n", 
        version_code_number);
    printf("*********************************************\n");
    printf("\n");

    _main_application_process();
}

int application_stop(void)
{
    _thread_aborted = true;
    
    // Add cleanup code here
    debug(tag, "Application stopped");


    hid_manager_deinit();
    modbus_manager_deinit();

    return 0;
}


int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    int ret = EXIT_SUCCESS;

    debug(tag, "Starting KENMEC application...");

    // Setup signal handlers
    if (signal(SIGINT, _handle_sigint) == SIG_ERR) {
        error(tag, "Cannot handle SIGINT!");
        exit(1);
    }

    if (signal(SIGTERM, _handle_sigint) == SIG_ERR) {
        error(tag, "Cannot handle SIGTERM!");
        exit(1);
    }

    application_run();

    return ret;
} 

/*

ifconfig eth0 192.168.1.100
umount /mnt/nfs
mount -t nfs -o nolock -o tcp 192.168.1.120:/home/kelvin/nfs /mnt/nfs

cp /mnt/nfs/kenmec_main ./

*/
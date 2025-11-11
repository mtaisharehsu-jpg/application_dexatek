#ifndef MAIN_APPLICATION_H
#define MAIN_APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif

void application_run(void);
void application_run_task(void);

int application_stop(void);

int libdexatek_version_get(uint8_t *major, uint8_t *minor, uint8_t *patch, uint8_t *version_code_number);

#ifdef __cplusplus
}
#endif

#endif

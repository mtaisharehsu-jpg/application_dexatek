
#ifndef LED_INDICATOR_H
#define LED_INDICATOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_INDICATOR_SETTING_OFF = 0,
    LED_INDICATOR_SETTING_ON = 1,
} LED_INDICATOR_SETTING;

// Initialization 
int led_indicator_init(void);

// Destroy 
int led_indicator_destroy(void);

// Update driver date
int led_indicator_update(void);

int led_indicator_set_red(BOOL value);
int led_indicator_set_green(BOOL value);
int led_indicator_set_blue(BOOL value);

#ifdef __cplusplus
}
#endif

#endif
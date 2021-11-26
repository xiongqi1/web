#ifndef __RENDER_h__
#define __RENDER_h__

#define LED_ENTRY_BRIGHTNESS "brightness"
#define LED_ENTRY_BRIGHTNESS_ON "brightness_on"
#define LED_ENTRY_BRIGHTNESS_OFF "brightness_off"
#define LED_ENTRY_DELAY_ON "delay_on"
#define LED_ENTRY_DELAY_OFF "delay_off"
#define LED_ENTRY_TRIGGER "trigger"
#define LED_ENTRY_TRIGGER_PULSE "trigger_pulse"
#define LED_ENTRY_TRIGGER_DEAD "trigger_dead"
#define LED_ENTRY_PATTERN "pattern"

#define LED_TRIGGER_BEAT "cdcs-beat"

#define LED_BRIGHTNESS_OFF 0
#define LED_BRIGHTNESS_FULL_ON 255

void led_set_solid(const char* led,int led_idx,int on);
void led_set_timer(const char* led,int led_idx,int on, int off);
void led_set_2timers(const char* led,const char* led2,int on, int off);
void led_touch_timer(const char* led,int led_idx);
void led_set_traffic(const char* led,int led_idx,const char* trigger,int on, int off);
int led_set_array(const char* leds[],const char* entry,const char* str,const char** strs,const int* vals);
int led_set_beat(const char *led, int led_idx, int brightness_on,
                 int brightness_off, const char *pattern, int tempo,
                 int rest);
int led_put_value(const char* led,const char* entry,const char* val_str,int val_int);
#endif

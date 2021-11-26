#ifndef TICK_CLOCK_H_02052018
#define TICK_CLOCK_H_02052018

#include <stdint.h>
#include <time.h>

typedef uint32_t time_ms_t;
typedef uint32_t time_diff_ms_t;

struct tick_clock_t {
    uint32_t interval_ms;
    uint32_t last_triggered_ms_valid;
    time_ms_t last_triggered_ms;
};

void tick_clock_update(void);
time_t tick_clock_get_time(void);
time_ms_t tick_clock_get_ms(void);
void tick_clock_do_trigger(struct tick_clock_t *tc);
time_ms_t tick_clock_get_duration(struct tick_clock_t *tc);
int tick_clock_is_expired(struct tick_clock_t *tc);
void tick_clock_reinit(struct tick_clock_t *tc, uint32_t interval_ms);
void tick_clock_init(struct tick_clock_t *tc, uint32_t interval_ms);
void tick_clock_fini(struct tick_clock_t *tc);

#endif


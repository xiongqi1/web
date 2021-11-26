/*!
 * Copyright Notice:
 * Copyright (C) 2012 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
*/

#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/types.h>

#include "rdb.h"
#include "led.h"
#include "dispd.h"
#include "stridx.h"

// add the following computer generated files
#include "dispd_def.h"

extern const struct str_to_idx_t rdbvar_to_varidx[];
extern const struct str_to_idx_t rdbval_to_validx[];
extern const enum disp_idx_t varidx_to_dispidx[];

static int rdbfd=-1;
struct disp_t disp[last_disp_ent]; // virtual LEDs that contains information
int verbosity=0;

#define DISP_ENT(idx) (&disp[(idx)])

#define DISP_ENT_DIRTY(idx) (DISP_ENT(idx)->dirty)
#define DISP_ENT_INT(idx) (DISP_ENT(idx)->no)
#define DISP_ENT_STR(idx) (DISP_ENT(idx)->str)

static struct stridx_t* si_varidx=NULL;
static struct stridx_t* si_validx=NULL;

#define TCK_PER_SEC		(sysconf(_SC_CLK_TCK))
#define BOOTUP_DELAY		40
#define DEFAULT_SELTIMEOUT	1000

#ifdef V_LEDFUN_falcon_1101
#define RSSI_LED_NUMBERS	2
#elif defined(V_LEDFUN_ntc_70) || defined(V_LEDFUN_ntc_20)
#define RSSI_LED_NUMBERS	0
#else
// The number of rssi leds is not 5 for nmc1000 which has only one physical led
// but define as 5 to give flexible threshold of green/red led colors
#define RSSI_LED_NUMBERS	5
#endif

#if defined(V_LEDFUN_140wx) || defined(V_LEDFUN_nwl22)
#define power_r "led1r"
#define power_g "led1g"
#define wlan_r	"led2r"
#define wlan_g  "led2g"
#define wwan_r	"led3r"
#define wwan_g	"led3g"
#define rssi1_r	 "led4r"
#define rssi1_g	 "led4g"
#define rssi2_r	 "led5r"
#define rssi2_g	 "led5g"
#define rssi3_r	 "led6r"
#define rssi3_g	 "led6g"
#define rssi4_r	 "led7r"
#define rssi4_g	 "led7g"
#define rssi5_r	 "led8r"
#define rssi5_g	 "led8g"
#endif

#if defined(V_LEDFUN_nwl22w)
#define power_r "led8r"
#define power_g "led8g"
#define wlan_r	"led5r"
#define wlan_g  "led5g"
#define wwan_r	"led4r"
#define wwan_g	"led4g"
#define rssi1_r	 "led6r"
#define rssi1_g	 "led6g"
#define rssi2_r	 "led7r"
#define rssi2_g	 "led7g"
#define rssi3_r	 "led1r"
#define rssi3_g	 "led1g"
#define rssi4_r	 "led2r"
#define rssi4_g	 "led2g"
#define rssi5_r	 "led3r"
#define rssi5_g	 "led3g"
#endif

#if defined(V_LEDFUN_145w)
#define power_r "led8r"
#define power_g "led8g"
#define wlan_r	"led4r"
#define wlan_g  "led4g"
#define wwan_r	"led5r"
#define wwan_g	"led5g"
#define rssi1_r	 "led6r"
#define rssi1_g	 "led6g"
#define rssi2_r	 "led7r"
#define rssi2_g	 "led7g"
#define rssi3_r	 "led1r"
#define rssi3_g	 "led1g"
#define rssi4_r	 "led2r"
#define rssi4_g	 "led2g"
#define rssi5_r	 "led3r"
#define rssi5_g	 "led3g"
#endif

#if defined(V_LEDFUN_140)
#define power_r "led1r"
#define power_g "led1g"
#define gps_r	"led2r"
#define gps_g   "led2g"
#define wwan_r	"led3r"
#define wwan_g	"led3g"
#define rssi1_r	 "led4r"
#define rssi1_g	 "led4g"
#define rssi2_r	 "led5r"
#define rssi2_g	 "led5g"
#define rssi3_r	 "led6r"
#define rssi3_g	 "led6g"
#define rssi4_r	 "led7r"
#define rssi4_g	 "led7g"
#define rssi5_r	 "led8r"
#define rssi5_g	 "led8g"
#endif

#if defined(V_LEDFUN_ntc_220)
#define power_r "led1r"
#define power_g "led1g"
#define wwan_r	"led2r"
#define wwan_g	"led2g"
#define gps_r	"led3r"
#define gps_g   "led3g"
#define rssi1_r	 "led4r"
#define rssi1_g	 "led4g"
#define rssi2_r	 "led5r"
#define rssi2_g	 "led5g"
#define rssi3_r	 "led6r"
#define rssi3_g	 "led6g"
#define rssi4_r	 "led7r"
#define rssi4_g	 "led7g"
#define rssi5_r	 "led8r"
#define rssi5_g	 "led8g"
#endif

#if defined(V_LEDFUN_wntd_idu)
#define status  "led_status_testing"
#define wwan	"led_wwan_status"
#define rssi1	"led_signal_low"
#define rssi2	"led_signal_med"
#define rssi3	"led_signal_high"
#endif

static clock_t cur=0; // current time (system tick)
static clock_t now=0; // current time (second)

static clock_t poweron_time;		// power on time
static clock_t screenon_time;		// led on time

static clock_t dimscreen_timer=0;	// LED off timer period

static int dirty_to_render=0;		// set 1 when rendering surface and disps are not matching

static int seltimeout=DEFAULT_SELTIMEOUT;	// ms second

struct led_pattern_t {
	const char* name;
	int br[32];
	int ms;
};

#if defined(V_LEDFUN_140wx) || defined(V_LEDFUN_nwl22) || defined(V_LEDFUN_140) || defined(V_LEDFUN_nwl22w) || defined(V_LEDFUN_145w) || defined(V_LEDFUN_ntc_220)
static int render_clear_leds(const char* leds[])
{
	led_set_array(leds,LED_ENTRY_TRIGGER,"none",NULL,NULL);
	led_set_array(leds,LED_ENTRY_BRIGHTNESS,"0",NULL,NULL);

	return 0;
}
#endif

// set brightness of multiple leds
static int render_led_pattern(const char* leds[],struct led_pattern_t* pat)
{
	// render led dance
	led_set_array(leds,LED_ENTRY_TRIGGER,"none",NULL,NULL);
	led_set_array(leds,LED_ENTRY_BRIGHTNESS,NULL,NULL,pat->br);

	// set name
	rdb_setVal("dispd.pattern.subtype",pat->name);

	return 0;
}


enum led_dance_stage {led_dance_stage_set,led_dance_stage_wait};


/*
	Falcon board pattern structures
*/

#if defined(V_LEDFUN_falcon_with_aux)
static const char* leds[]={
	"power_g",
	"wwan_g",
 	"aux_g",
	"rssi1_g",
	"rssi2_g",
	"rssi3_g",
	"rssi4_g",
	"rssi5_g",
	"power_r",
	"wwan_r",
 	"aux_r",
	"rssi1_r",
	"rssi2_r",
	"rssi3_r",
	"rssi4_r",
	"rssi5_r",
	NULL
};

struct led_pattern_t pats_reset[]={
	// main - green light (total 5 seconds)
	{"main",{1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,1,1,1,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,1,1,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,0,1,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,0,0,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,0,0,0,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,0,0,0,0,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},5000/9},
	// recovery - yellow (total 10 seconds)
	{"recovery",{1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1},10000/9},
	{"recovery",{0,1,1,1,1,1,1,1, 0,1,1,1,1,1,1,1},10000/9},
	{"recovery",{0,0,1,1,1,1,1,1, 0,0,1,1,1,1,1,1},10000/9},
	{"recovery",{0,0,0,1,1,1,1,1, 0,0,0,1,1,1,1,1},10000/9},
	{"recovery",{0,0,0,0,1,1,1,1, 0,0,0,0,1,1,1,1},10000/9},
	{"recovery",{0,0,0,0,0,1,1,1, 0,0,0,0,0,1,1,1},10000/9},
	{"recovery",{0,0,0,0,0,0,1,1, 0,0,0,0,0,0,1,1},10000/9},
	{"recovery",{0,0,0,0,0,0,0,1, 0,0,0,0,0,0,0,1},10000/9},
	{"recovery",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},10000/9},
	// factory - red ( total 5 seconds )
	{"factory",{0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,1,1,1,1,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,1,1,1,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,1,1,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,0,1,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,0,0,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},5000/9},

	// end
	{NULL,     {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},0},
};

struct led_pattern_t pats_mainflashing[]={
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	// end
	{NULL,      {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},0},
};

struct led_pattern_t pats_recoveryflashing[]={
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	// end
	{NULL,      {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},0},
};

struct led_pattern_t pats_factoryflashing[]={
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	// end
	{NULL,      {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},0},
};

#elif defined(V_LEDFUN_falcon_default)
static const char* leds[]={
	"power_g",
	"wwan_g",
	"rssi1_g",
	"rssi2_g",
	"rssi3_g",
	"rssi4_g",
	"rssi5_g",
	"power_r",
	"wwan_r",
	"rssi1_r",
	"rssi2_r",
	"rssi3_r",
	"rssi4_r",
	"rssi5_r",
	NULL
};

struct led_pattern_t pats_reset[]={
	// main - green light (total 5 seconds)
	{"main",{1,1,1,1,1,1,1, 0,0,0,0,0,0,0},5000/8},
	{"main",{0,1,1,1,1,1,1, 0,0,0,0,0,0,0},5000/8},
	{"main",{0,0,1,1,1,1,1, 0,0,0,0,0,0,0},5000/8},
	{"main",{0,0,0,1,1,1,1, 0,0,0,0,0,0,0},5000/8},
	{"main",{0,0,0,0,1,1,1, 0,0,0,0,0,0,0},5000/8},
	{"main",{0,0,0,0,0,1,1, 0,0,0,0,0,0,0},5000/8},
	{"main",{0,0,0,0,0,0,1, 0,0,0,0,0,0,0},5000/8},
	{"main",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},5000/8},
	// recovery - yellow (total 10 seconds)
	{"recovery",{1,1,1,1,1,1,1, 1,1,1,1,1,1,1},10000/8},
	{"recovery",{0,1,1,1,1,1,1, 0,1,1,1,1,1,1},10000/8},
	{"recovery",{0,0,1,1,1,1,1, 0,0,1,1,1,1,1},10000/8},
	{"recovery",{0,0,0,1,1,1,1, 0,0,0,1,1,1,1},10000/8},
	{"recovery",{0,0,0,0,1,1,1, 0,0,0,0,1,1,1},10000/8},
	{"recovery",{0,0,0,0,0,1,1, 0,0,0,0,0,1,1},10000/8},
	{"recovery",{0,0,0,0,0,0,1, 0,0,0,0,0,0,1},10000/8},
	{"recovery",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},10000/8},
	// factory - red ( total 5 seconds )
	{"factory",{0,0,0,0,0,0,0, 1,1,1,1,1,1,1},5000/8},
	{"factory",{0,0,0,0,0,0,0, 0,1,1,1,1,1,1},5000/8},
	{"factory",{0,0,0,0,0,0,0, 0,0,1,1,1,1,1},5000/8},
	{"factory",{0,0,0,0,0,0,0, 0,0,0,1,1,1,1},5000/8},
	{"factory",{0,0,0,0,0,0,0, 0,0,0,0,1,1,1},5000/8},
	{"factory",{0,0,0,0,0,0,0, 0,0,0,0,0,1,1},5000/8},
	{"factory",{0,0,0,0,0,0,0, 0,0,0,0,0,0,1},5000/8},
	{"factory",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},5000/8},

	// end
	{NULL,     {0,0,0,0,0,0,0, 0,0,0,0,0,0,0},0},
};

struct led_pattern_t pats_mainflashing[]={
	{"flashing",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1, 0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1, 0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},50},
	// end
	{NULL,     {0,0,0,0,0,0,0, 0,0,0,0,0,0,0},0},
};

struct led_pattern_t pats_recoveryflashing[]={
	{"flashing",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1, 1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1, 1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},50},
	// end
	{NULL,     {0,0,0,0,0,0,0, 0,0,0,0,0,0,0},0},
};

struct led_pattern_t pats_factoryflashing[]={
	{"flashing",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0, 1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0, 1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0, 0,0,0,0,0,0,0},50},
	// end
	{NULL,     {0,0,0,0,0,0,0, 0,0,0,0,0,0,0},0},
};

#elif defined(V_LEDFUN_falcon_1101)
static const char* leds[]={
	"wwan_g",
	"rssi1_g",
	"rssi2_g",
	NULL
};

struct led_pattern_t pats_reset[]={
	// main - green light (total 5 seconds)
	{"main",{1,1,1},5000/8},
	{"main",{0,1,1},5000/8},
	{"main",{0,0,1},5000/8},
	{"main",{0,0,0},5000/8},
	// recovery - green (total 10 seconds)
	{"recovery",{1,1,1},5000/8},
	{"recovery",{0,1,1},5000/8},
	{"recovery",{0,0,1},5000/8},
	{"recovery",{0,0,0},5000/8},
	// factory - green ( total 5 seconds )
	{"factory",{1,1,1},5000/8},
	{"factory",{0,1,1},5000/8},
	{"factory",{0,0,1},5000/8},
	{"factory",{0,0,0},5000/8},
	// end
	{NULL,     {0,0, 0,0},0},
};

struct led_pattern_t pats_mainflashing[]={
	{"flashing",{0,0,0},50},
	{"flashing",{1,1,1},50},
	{"flashing",{0,0,0},50},
	{"flashing",{1,1,1},50},
	{"flashing",{0,0,0},50},
	// end
	{NULL,     {0,0, 0,0},0},
};

struct led_pattern_t pats_recoveryflashing[]={
	{"flashing",{0,0,0},50},
	{"flashing",{1,1,1},50},
	{"flashing",{0,0,0},50},
	{"flashing",{1,1,1},50},
	{"flashing",{0,0,0},50},
	// end
	{NULL,     {0,0, 0,0,},0},
};

struct led_pattern_t pats_factoryflashing[]={
	{"flashing",{0,0,0},50},
	{"flashing",{1,1,1},50},
	{"flashing",{0,0,0},50},
	{"flashing",{1,1,1},50},
	{"flashing",{0,0,0},50},
	// end
	{NULL,     {0,0, 0,0},0},
};

#elif defined(V_LEDFUN_nmc1000) || defined(V_LEDFUN_ntc_70)
#if defined(V_LEDFUN_nmc1000)
static const char* leds[]={
	"power_g",
	"wwan_g",
	"exp_g",
	"rssi1_g",
	"power_r",
	"wwan_r",
	"exp_r",
	"rssi1_r",
	NULL
};
#elif defined(V_LEDFUN_ntc_70)
static const char* leds[]={
	"power_g",
	"wwan_g",
	"zigbee_g",
	"wlan_g",
	"power_r",
	"wwan_r",
	"zigbee_r",
	"wlan_r",
	NULL
};
#else
#error Check V_LEDFUN
#endif

struct led_pattern_t pats_reset[]={
	// main - green light (total 5 seconds)
	{"main",{1,1,1,1, 0,0,0,0},5000/5},
	{"main",{0,1,1,1, 0,0,0,0},5000/5},
	{"main",{0,0,1,1, 0,0,0,0},5000/5},
	{"main",{0,0,0,1, 0,0,0,0},5000/5},
	{"main",{0,0,0,0, 0,0,0,0},5000/5},
	// recovery - yellow (total 10 seconds)
	{"recovery",{1,1,1,1, 1,1,1,1},10000/5},
	{"recovery",{0,1,1,1, 0,1,1,1},10000/5},
	{"recovery",{0,0,1,1, 0,0,1,1},10000/5},
	{"recovery",{0,0,0,1, 0,0,0,1},10000/5},
	{"recovery",{0,0,0,0, 0,0,0,0},10000/5},
	// factory - red ( total 5 seconds )
	{"factory",{0,0,0,0, 1,1,1,1},5000/5},
	{"factory",{0,0,0,0, 0,1,1,1},5000/5},
	{"factory",{0,0,0,0, 0,0,1,1},5000/5},
	{"factory",{0,0,0,0, 0,0,0,1},5000/5},
	{"factory",{0,0,0,0, 0,0,0,0},5000/5},

	// end
	{NULL,     {0,0,0,0, 0,0,0,0},0},
};

struct led_pattern_t pats_mainflashing[]={
	{"flashing",{0,0,0,0, 0,0,0,0},50},
	{"flashing",{1,1,1,1, 0,0,0,0},50},
	{"flashing",{0,0,0,0, 0,0,0,0},50},
	{"flashing",{1,1,1,1, 0,0,0,0},50},
	{"flashing",{0,0,0,0, 0,0,0,0},50},
	// end
	{NULL,     {0,0,0,0, 0,0,0,0},0},
};

struct led_pattern_t pats_recoveryflashing[]={
	{"flashing",{0,0,0,0, 0,0,0,0},50},
	{"flashing",{1,1,1,1, 1,1,1,1},50},
	{"flashing",{0,0,0,0, 0,0,0,0},50},
	{"flashing",{1,1,1,1, 1,1,1,1},50},
	{"flashing",{0,0,0,0, 0,0,0,0},50},
	// end
	{NULL,     {0,0,0,0, 0,0,0,0},0},
};

struct led_pattern_t pats_factoryflashing[]={
	{"flashing",{0,0,0,0, 0,0,0,0},50},
	{"flashing",{0,0,0,0, 1,1,1,1},50},
	{"flashing",{0,0,0,0, 0,0,0,0},50},
	{"flashing",{0,0,0,0, 1,1,1,1},50},
	{"flashing",{0,0,0,0, 0,0,0,0},50},
	// end
	{NULL,     {0,0,0,0, 0,0,0,0},0},
};

#elif defined(V_LEDFUN_ntc_20)
static const char* leds[]={
	"power_g",
	"power_r",
	NULL
};

struct led_pattern_t pats_reset[]={
	// main - green light (total 5 seconds)
	{"main",{1,0},5000/8},
	{"main",{0,0},5000/8},
	{"main",{0,0},5000/8},
	{"main",{0,0},5000/8},
	{"main",{0,0},5000/8},
	// recovery - yellow (total 10 seconds)
	{"recovery",{1,1},10000/8},
	{"recovery",{0,0},10000/8},
	{"recovery",{0,0},10000/8},
	{"recovery",{0,0},10000/8},
	{"recovery",{0,0},10000/8},
	// factory - red ( total 5 seconds )
	{"factory",{0,1},5000/8},
	{"factory",{0,0},5000/8},
	{"factory",{0,0},5000/8},
	{"factory",{0,0},5000/8},
	{"factory",{0,0},5000/8},

	// end
	{NULL,     {0,0},0},
};

struct led_pattern_t pats_mainflashing[]={
	{"flashing",{0,0},50},
	{"flashing",{1,0},50},
	{"flashing",{0,0},50},
	{"flashing",{1,0},50},
	{"flashing",{0,0},50},
	// end
	{NULL,     {0,0,0,0, 0,0,0,0},0},
};

struct led_pattern_t pats_recoveryflashing[]={
	{"flashing",{0,0},50},
	{"flashing",{1,1},50},
	{"flashing",{0,0},50},
	{"flashing",{1,1},50},
	{"flashing",{0,0},50},
	// end
	{NULL,      {0,0},0},
};

struct led_pattern_t pats_factoryflashing[]={
	{"flashing",{0,0},50},
	{"flashing",{0,1},50},
	{"flashing",{0,0},50},
	{"flashing",{0,1},50},
	{"flashing",{0,0},50},
	// end
	{NULL,     {0,0},0},
};

struct led_pattern_t pats_error[]={
    {"error",{0,1},100},
	// end
	{NULL,     {0,0},0},
};

struct led_pattern_t pats_msgalert[]={
	{"msgalert",{1,1},100},
	{"msgalert",{0,0},100},
	{"msgalert",{1,1},100},
	{"msgalert",{0,0},4700},
	// end
	{NULL,     {0,0},0},
};

struct led_pattern_t pats_booting[]={
	{"booting",{1,0},100},
	{"booting",{0,0},100},
	// end
	{NULL,     {0,0},0},
};

struct led_pattern_t pats_success[]={
	{"success",{1,0},1000},
	{"success",{0,0},100},
	// end
	{NULL,     {0,0},0},
};

#elif defined(V_LEDFUN_140wx) || defined(V_LEDFUN_140) || defined(V_LEDFUN_nwl22) || defined(V_LEDFUN_nwl22w) || defined(V_LEDFUN_145w) || defined(V_LEDFUN_ntc_220)
/* WARNING: placeholder, this doesn't actually match the LEDs */
#if defined(V_LEDFUN_140)
static const char* leds[]={
	power_g,
	gps_g,
	wwan_g,
	rssi1_g,
	rssi2_g,
	rssi3_g,
	rssi4_g,
	rssi5_g,
	power_r,
	gps_r,
	wwan_r,
	rssi1_r,
	rssi2_r,
	rssi3_r,
	rssi4_r,
	rssi5_r,
	NULL
};
#elif defined(V_LEDFUN_ntc_220)
static const char* leds[]={
	power_g,
	wwan_g,
	gps_g,
	rssi1_g,
	rssi2_g,
	rssi3_g,
	rssi4_g,
	rssi5_g,
	power_r,
	wwan_r,
	gps_r,
	rssi1_r,
	rssi2_r,
	rssi3_r,
	rssi4_r,
	rssi5_r,
	NULL
};
#else
static const char* leds[]={
	power_g,
	wlan_g,
	wwan_g,
	rssi1_g,
	rssi2_g,
	rssi3_g,
	rssi4_g,
	rssi5_g,
	power_r,
	wlan_r,
	wwan_r,
	rssi1_r,
	rssi2_r,
	rssi3_r,
	rssi4_r,
	rssi5_r,
	NULL
};
#endif
struct led_pattern_t pats_reset[]={
	// main - green light (total 5 seconds)
	{"main",{1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,1,1,1,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,1,1,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,0,1,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,0,0,1,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,0,0,0,1,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,0,0,0,0,1, 0,0,0,0,0,0,0,0},5000/9},
	{"main",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},5000/9},
	// recovery - yellow(total 10 seconds)
	{"recovery",{1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1},10000/9},
	{"recovery",{0,1,1,1,1,1,1,1, 0,1,1,1,1,1,1,1},10000/9},
	{"recovery",{0,0,1,1,1,1,1,1, 0,0,1,1,1,1,1,1},10000/9},
	{"recovery",{0,0,0,1,1,1,1,1, 0,0,0,1,1,1,1,1},10000/9},
	{"recovery",{0,0,0,0,1,1,1,1, 0,0,0,0,1,1,1,1},10000/9},
	{"recovery",{0,0,0,0,0,1,1,1, 0,0,0,0,0,1,1,1},10000/9},
	{"recovery",{0,0,0,0,0,0,1,1, 0,0,0,0,0,0,1,1},10000/9},
	{"recovery",{0,0,0,0,0,0,0,1, 0,0,0,0,0,0,0,1},10000/9},
	{"recovery",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},10000/9},
	// factory - red ( total 5 seconds )
	{"factory",{0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,1,1,1,1,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,1,1,1,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,1,1,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,0,1,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,0,0,1,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,1,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1},5000/9},
	{"factory",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},5000/9},
	// end
	{NULL,     {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},0},
};

struct led_pattern_t pats_mainflashing[]={
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	// end
	{NULL,     {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},0},
};

struct led_pattern_t pats_recoveryflashing[]={
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	// end
	{NULL,     {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},0},
};

struct led_pattern_t pats_factoryflashing[]={
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	{"flashing",{0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1},50},
	{"flashing",{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},50},
	// end
	{NULL,     {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},0},
};

#elif defined(V_LEDFUN_wntd_idu)
/* WNTD devices consist of an outdoor unit and a separate indoor unit
 * The LEDs are on the indoor unit and are controlled by a separate 'indoorunit_mgr'
 *
 * To facilitate this we build idu_led.c rather than led.c
 * idu_led.c implents the same functions as led.c but outputs to rdb variables
 * rather than sysfs.
 *
 * The downside of this approach is that if we want to support LEDs on both the ODU
 * and IDU we would not be able to control the ODU LEDs using the usual sysfs method.
 *
 * At present that's not a problem as the ODU has no LEDs.
 *
 * However if needed in the future we could implement a simple handler in lua (or whatever)
 * to control the ODU LEDs when certain RDB variables are set by dispd.
 */
#define IDU_LEDS_ONLY

#else
#error LED render of the board is not defined!
#endif

// local functions
static void set_disp_ent_int(enum disp_idx_t idx,int val);
static void set_disp_ent_str(enum disp_idx_t idx,const char* str);

#ifndef IDU_LEDS_ONLY

struct {
	struct led_pattern_t* pats;
	int loop;
} pats_info[] = {
	{pats_reset,0}, // resetbutton
	{pats_mainflashing,1}, // mainflashing
	{pats_recoveryflashing,1}, // recoveryflashing
	{pats_factoryflashing,1}, // factoryflashing
#if defined(V_LEDFUN_ntc_20)
	{pats_error,1}, // error
	{pats_msgalert,0}, // msgalert
	{pats_booting,0}, // booting
	{pats_success,1}, // success
#endif
};


#if defined(V_LEDFUN_140wx) || defined(V_LEDFUN_140) || defined(V_LEDFUN_nwl22) || defined(V_LEDFUN_nwl22w) || defined(V_LEDFUN_ntc_20) || defined(V_LEDFUN_145w) || defined(V_LEDFUN_ntc_220)
static int process_led_dance()
{
	// process led dance
	int dancing;
	int newtype;
	static int type=-1;
	static int step=0;
	static int loop=0;

	static clock_t danceon_time=0;
	static enum led_dance_stage stage=led_dance_stage_set;

	// if pattern dance is triggered
	if(DISP_ENT_DIRTY(pattern_dance) || DISP_ENT_DIRTY(dispd_disable)) {
		newtype=DISP_ENT_INT(pattern_dance);

		if(newtype<0) {
			if(type>=0)
				syslog(LOG_INFO,"[led-pattern] stop a pattern");
		}
		else {
			loop=0;
			step=0;
			stage=led_dance_stage_set;

			syslog(LOG_INFO,"[led-pattern] start a pattern - %s",DISP_ENT_STR(pattern_dance));
		}

		type=newtype;
	}

	// get dancing flag
	dancing=type>=0;

	// use intensive and aggressive select timeout for dancing pattern
	seltimeout=dancing?10:DEFAULT_SELTIMEOUT;

	// keep on dancing in dance mode
	if(dancing) {
		struct led_pattern_t* pats;

		pats=pats_info[type].pats;

		switch(stage) {
			case led_dance_stage_set:
				syslog(LOG_DEBUG,"[led-pattern] set pattern - type=%d,name=%s,time=%d,step=%d,loop=%d",type,pats[step].name,pats[step].ms,step,loop);
				render_led_pattern(leds,&pats[step]);

				danceon_time=cur;
				stage=led_dance_stage_wait;
				break;

			case led_dance_stage_wait:
				if(!pats[step].name)
					return -1;

				if(cur-danceon_time<pats[step].ms*TCK_PER_SEC/1000)
					break;

				step=step+1;

				// rewind if we are at the end
				if(!pats[step].name) {
					loop++;

					// we don't dance any more if it exceeds loop count
					// the loop continues forever when loop is zero.
					if(pats_info[type].loop && (loop>=pats_info[type].loop)) {
						syslog(LOG_INFO,"[led-pattern] loop pattern done");
						return -1;
					}

					step=0;
				}

				stage=led_dance_stage_set;

				syslog(LOG_DEBUG,"[led-pattern] move to the next step - step=%d",step);
				break;
		}

		return -1;
	}
	return 0;
}
#endif

#endif

#if defined(V_BATTERY_y)

#define BATTERY_LOW_THRESHOLD 15
static bool
is_battery_low (void)
{
    return (DISP_ENT_INT(battery_online) &&
            DISP_ENT_INT(battery_status) != RDB_VAL(BATTERY_STATUS, C) &&
            atoi(DISP_ENT_STR(battery_percent)) <= BATTERY_LOW_THRESHOLD);
}
#else /* !defined(V_BATTERY_Y) */
static bool
is_battery_low (void)
{
    return false;
}
#endif /* V_BATTERY_y */

#if defined(V_LEDFUN_wntd_idu)
/*
 * Render virtual LEDs to physical LEDs for WNTD devices
 *
 * The LEDs are controlled via the following RDB variables:-
 *
 * indoor.led_status_testing
 * indoor.led_wwan_status
 * indoor.led_signal_low
 * indoor.led_signal_med
 * indoor.led_signal_high
 *
 * The IDU 'status' LED is primarily conrolled by the IDU itself however
 * indoor.led_status_testing can be used to cause the 'status' LED to blink
 * to indicate 'test' mode.
 *
 *
 * Status LED is special
 * It only supports remote control of blinking/non blinking
 *
 * At Jamie's suggestion we define not blinking as normal mode
 * And blinking as test mode
 *
 */

#define INDOOR_LED_STATE_NONE  			0
#define INDOOR_LED_STATE_RED   			1
#define INDOOR_LED_STATE_GREEN			2
#define INDOOR_LED_STATE_AMBER 			3
#define INDOOR_LED_STATE_BLINK			4

void led_set_idu(const char* led, int state)
{
	char ledrdbname[128];
	char ledval[32];

	// bypass if led is held
	//if(is_led_held(led,led_idx))
	//	return;

	syslog(LOG_DEBUG,"[LED] setting led(%s) - val=%d",led,state);

	snprintf(ledrdbname,sizeof(ledrdbname),"indoor.%s",led);
	snprintf(ledval,sizeof(ledval),"%d",state);

	rdb_setVal(ledrdbname,ledval);
}

static int render_disps_wntd_idu()
{
	// power led - router_power_status
	if(DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(booting_delay) || DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(boot_mode) || DISP_ENT_DIRTY(sim_card_status) || DISP_ENT_DIRTY(module_name)) {

		int recovery;
		int sim_not_inserted;

		int hwf_module;

		recovery=*DISP_ENT_STR(boot_mode);

		// apply all 2g/3g/lte hardware failure in production mode
		sim_not_inserted=!recovery && (DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_NOT_INSERTED));
		hwf_module=!recovery && !DISP_ENT_INT(booting_delay) && !*DISP_ENT_STR(module_name);

		// powering up
		if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to power leds");

			led_set_idu(status,1); // We can't actually trun off the status led.
		}

		// Hardware error
		else if (sim_not_inserted || hwf_module) {

			if(sim_not_inserted)
				syslog(LOG_ERR,"[hardware failure] no sim card detected");
			if(hwf_module)
				syslog(LOG_ERR,"[hardware failure] no phone module detected");

			syslog(LOG_DEBUG,"[render] set hardware failure led - sim=%d,module=%d",sim_not_inserted,hwf_module);

			//led_set_timer(power_r,0,2000,2000);
			led_set_idu(status,INDOOR_LED_STATE_RED | INDOOR_LED_STATE_BLINK);
		}

		// power on in recovery mode
		else if(recovery) {
			syslog(LOG_DEBUG,"[render] set power led (recovery=%d)",recovery);
			//led_set_idu(power_r,0,255);
			led_set_idu(status,0);
		}

		// power on
		else {
			syslog(LOG_DEBUG,"[render] set power led ");
			//led_set_idu(power_r,0,0);
			led_set_idu(status,1);
		}
	}

	// Network led - wwan_online_status
	if(DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(sim_card_status) || DISP_ENT_DIRTY(module_name) || DISP_ENT_DIRTY(wwan_enable_status) || DISP_ENT_DIRTY(wwan_connecting_status) || DISP_ENT_DIRTY(wwan_online_status) || DISP_ENT_DIRTY(cdma_activated) || DISP_ENT_DIRTY(wwan_reg) || DISP_ENT_DIRTY(booting_delay)) {
		int online;
		int offline;
		int connecting;

		int reg;
		int registered;
		int registering;

		int sim_puk;
		int sim_pin;
		int sim_not_inserted;

		int module_ready;
		int not_activated;

		// apply all 3g hardware failure in production mode
		module_ready=*DISP_ENT_STR(module_name);

		// get online and connecting status
		online=DISP_ENT_INT(wwan_online_status)==1;
		offline=DISP_ENT_INT(wwan_online_status)==0;
		connecting=DISP_ENT_INT(wwan_connecting_status);

		// get cdma activation status
		not_activated=DISP_ENT_INT(cdma_activated)==0;

		// get network activity
		reg=atoi(DISP_ENT_STR(wwan_reg));
		registered=(reg==1) || (reg>=5);
		registering=(reg==2);

		// get sim status
		sim_puk=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_PUK));
		sim_pin=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_PIN));
		sim_not_inserted=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_NOT_INSERTED)) || (DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,NEGOTIATING));

		if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to wwan leds");

			//led_set_idu(wwan_r,0,0);
			led_set_idu(wwan,0);
		}
		// Can't connect
		else if(sim_not_inserted && module_ready) {
			syslog(LOG_DEBUG,"[render] set sim-not-inserted led");

			//led_set_idu(wwan_r,0,255);
			led_set_idu(wwan,INDOOR_LED_STATE_RED);
		}
		// SIM PUK locked
		else if(sim_puk && module_ready) {
			syslog(LOG_DEBUG,"[render] set SIM PUK led");
			led_set_idu(wwan,INDOOR_LED_STATE_RED);
		}
		// SIM PIN locked
		else if(sim_pin && !DISP_ENT_INT(booting_delay) && module_ready) {
			syslog(LOG_DEBUG,"[render] set SIM PIN led");
			led_set_idu(wwan,INDOOR_LED_STATE_RED|INDOOR_LED_STATE_BLINK);
		}
		// Connected and Traffic via wwan
		else if(online && module_ready) {
			syslog(LOG_DEBUG,"[render] set online led");
			led_set_idu(wwan,INDOOR_LED_STATE_GREEN);
		}
		// Connecting PDP
		else if((connecting==1) && module_ready) {
			syslog(LOG_DEBUG,"[render] set connecting led");
			led_set_idu(wwan,INDOOR_LED_STATE_AMBER|INDOOR_LED_STATE_BLINK);
		}
		// Registered network
		else if(registered && !offline && module_ready) {
			syslog(LOG_DEBUG,"[render] set registered led");
			led_set_idu(wwan,INDOOR_LED_STATE_AMBER);
		}
		// Registering network
		else if(registering && module_ready) {
			syslog(LOG_DEBUG,"[render] set registering led");
			led_set_idu(wwan,INDOOR_LED_STATE_AMBER|INDOOR_LED_STATE_BLINK);
		}
		else if(!DISP_ENT_INT(wwan_enable_status)) {
			syslog(LOG_DEBUG,"[render] set wwan disable led");
			led_set_idu(wwan,INDOOR_LED_STATE_NONE);
		}
		// hold on the red led during the boot up period
		else if(DISP_ENT_INT(booting_delay)) {
		}
		else if(connecting==0) {
			syslog(LOG_DEBUG,"[render] set offline led");
			led_set_idu(wwan,INDOOR_LED_STATE_NONE);
		}
	}


	// rssi led - signal_strength & service_type
	// power led - router_power_status
	if(DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(service_type) || DISP_ENT_DIRTY(signal_strength)) {
		int gsm;
		int gprs;
		int umts;
		int lte;
		int bars;
		int rssi_thresholds_wcdma[RSSI_LED_NUMBERS]={-109,-101,-91,-85,-77};
		int rssi_thresholds_gsm[RSSI_LED_NUMBERS]={-109,-102,-93,-87,-78};
		int rsrp_thresholds_lte[RSSI_LED_NUMBERS]={-110,-100,-90,-80,-70};
		int sig_strength;
		int i;

		// use RAT (^SMONI for Cinterion)
		gprs=(DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,EGPRS)) // EGPRS
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GPRS)) // GPRS
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XRTT)) // CDMA 1x
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,DC_HSPAP)) // DC-HSPA+
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,HSPAP)); // HSPA+

		gsm=DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GSM)
				|| DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GSM_COMPACT)
				|| DISP_ENT_INT(service_type)==-1; // GSM

		lte=DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,LTE); // LTE
		umts=!gprs && !gsm && !lte;
		/* set evdo */
		umts=umts \
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XEVDO_RELEASE_0))
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XEVDO_RELEASE_A))
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,UMTS));

		// get signal strength(rssi or rsrp)
		sig_strength=atoi(DISP_ENT_STR(signal_strength));
		// get bars
		bars=0;
		for(i=0;i<RSSI_LED_NUMBERS;i++) {
			if(gsm && sig_strength && (rssi_thresholds_gsm[i]<=sig_strength))
				bars++;
			else if((umts || gprs) && sig_strength && (rssi_thresholds_wcdma[i]<=sig_strength))
				bars++;
			else if(lte && sig_strength && (rsrp_thresholds_lte[i]<=sig_strength))
				bars++;
		}

		syslog(LOG_DEBUG,"[disp] rssi=%d,service_type=%s,bars=%d,gsm=%d,grps=%d,umts=%d,lte=%d",sig_strength,DISP_ENT_STR(service_type),bars,gsm,gprs,umts,lte);

		if(DISP_ENT_INT(dim_screen)){
			syslog(LOG_DEBUG,"[led-off] dim-mode to rssi leds");
			led_set_idu(rssi1,INDOOR_LED_STATE_NONE);
			led_set_idu(rssi2,INDOOR_LED_STATE_NONE);
			led_set_idu(rssi3,INDOOR_LED_STATE_NONE);
		}

		switch (bars) {
			case 0:
				led_set_idu(rssi1,INDOOR_LED_STATE_NONE);
				led_set_idu(rssi2,INDOOR_LED_STATE_NONE);
				led_set_idu(rssi3,INDOOR_LED_STATE_NONE);
				break;
			case 1:
				led_set_idu(rssi1,INDOOR_LED_STATE_RED);
				led_set_idu(rssi2,INDOOR_LED_STATE_NONE);
				led_set_idu(rssi3,INDOOR_LED_STATE_NONE);
				break;
			case 2:
				led_set_idu(rssi1,INDOOR_LED_STATE_AMBER);
				led_set_idu(rssi2,INDOOR_LED_STATE_NONE);
				led_set_idu(rssi3,INDOOR_LED_STATE_NONE);
				break;
			case 3:
				led_set_idu(rssi1,INDOOR_LED_STATE_AMBER);
				led_set_idu(rssi2,INDOOR_LED_STATE_AMBER);
				led_set_idu(rssi3,INDOOR_LED_STATE_NONE);
				break;
			case 4:
				led_set_idu(rssi1,INDOOR_LED_STATE_GREEN);
				led_set_idu(rssi2,INDOOR_LED_STATE_GREEN);
				led_set_idu(rssi3,INDOOR_LED_STATE_NONE);
				break;
			case 5:
				led_set_idu(rssi1,INDOOR_LED_STATE_GREEN);
				led_set_idu(rssi2,INDOOR_LED_STATE_GREEN);
				led_set_idu(rssi3,INDOOR_LED_STATE_GREEN);
				break;
			default:
				syslog(LOG_ERR,"Cannot display %d bars", bars);
				break;
		}
	}
	return 0;
}
#endif

#if defined(V_LEDFUN_140wx) || defined(V_LEDFUN_140) ||  defined(V_LEDFUN_nwl22) || defined(V_LEDFUN_ntc_220)
/*
	render for nguni board like 140wx- display disps (virtual LEDs - information) to physical (LEDs) for nguni board
*/
static int render_disps_nguni()
{
	int i;
	// process led dance
	i = process_led_dance();

	if(i == -1) {
		return i;
	}

    const bool in_factory_reset_mode = (
#if defined(V_REQUIRE_FACTORY_PASSWORD_CHANGE_y)
        DISP_ENT_INT(factory_reset_mode) == RDB_VAL(ADMIN_FACTORY_DEFAULT_PASSWORDS_IN_USE, 1)
#else
	0
#endif
    );

	// power led - router_power_status
	if(
	    DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(booting_delay) ||
	    DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(boot_mode) ||
	    DISP_ENT_DIRTY(sim_card_status) || DISP_ENT_DIRTY(module_name) ||
	    DISP_ENT_DIRTY(factory_reset_mode)
	) {

		int recovery;
		int sim_not_inserted;

		int hwf_module;

		recovery=*DISP_ENT_STR(boot_mode);

		// apply all 2g/3g/lte hardware failure in production mode
		sim_not_inserted=!recovery && (DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_NOT_INSERTED));
		hwf_module=!recovery && !DISP_ENT_INT(booting_delay) && !*DISP_ENT_STR(module_name);
		// powering up
		if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to power leds");

			led_set_solid(power_g,0,0);
				led_set_solid(power_r,0,0);
		}
		else if (in_factory_reset_mode) {
			syslog(LOG_DEBUG,"[led-off] factory default mode");
			led_set_solid(power_r,0,0);
			led_set_solid(power_g,0,255);
		}
		// Hardware error
		else if (sim_not_inserted || hwf_module) {

			if(sim_not_inserted)
				syslog(LOG_ERR,"[hardware failure] no sim card detected");
			if(hwf_module)
				syslog(LOG_ERR,"[hardware failure] no phone module detected");

			syslog(LOG_DEBUG,"[render] set hardware failure led - sim=%d,module=%d",sim_not_inserted,hwf_module);

			led_set_timer(power_r,0,2000,2000);
			led_set_solid(power_g,0,0);
		}
		// power on in recovery mode
		else if(recovery) {
			syslog(LOG_DEBUG,"[render] set power led (recovery=%d)",recovery);
			led_set_solid(power_r,0,255);
			led_set_solid(power_g,0,255);
		}
		// power on
		else {
			syslog(LOG_DEBUG,"[render] set power led ");
			led_set_solid(power_r,0,0);
			led_set_solid(power_g,0,255);
		}
	}
	#if defined(V_LEDFUN_140) || defined(V_LEDFUN_ntc_220)
	#if defined(V_GPS_y)
	// gps led - satellite_data
	if(
	    DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(pattern_dance) ||
	    DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(satellite_valid) ||
	    DISP_ENT_DIRTY(gps_enable) || DISP_ENT_DIRTY(satellite_count) ||
	    DISP_ENT_DIRTY(factory_reset_mode)
	) {
		int st_count;

		int period;
		const int period_max=1500;
		const int period_min=100;

		/* get satellite count */
		st_count=atoi(DISP_ENT_STR(satellite_count));

		/* dim screen */
		if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to gps led");

			led_set_solid(gps_r,0,0);
			led_set_solid(gps_g,0,0);
		}
		else if (in_factory_reset_mode) {
			syslog(LOG_DEBUG,"[led-off] factory default mode");
			led_set_solid(gps_r,0,0);
			led_set_solid(gps_g,0,0);
		}
		/* gps disabled */
		else if(DISP_ENT_INT(gps_enable)<0 || DISP_ENT_INT(gps_enable)==RDB_VAL(SENSORS_GPS_0_ENABLE,0)) {
			syslog(LOG_DEBUG,"[render] GPS disabled");

			led_set_solid(gps_r,0,0);
			led_set_solid(gps_g,0,0);
		}
		/* gps aquired */
		else if(DISP_ENT_INT(satellite_valid)==RDB_VAL(SENSORS_GPS_0_STANDALONE_VALID,VALID)) {
			syslog(LOG_DEBUG,"[render] GPS acquired");

			led_set_solid(gps_r,0,0);
			led_set_solid(gps_g,0,255);
		}
		/* no satellite found */
		else if(st_count==0) {
			syslog(LOG_DEBUG,"[render] no satellite found");
			led_set_solid(gps_r,0,255);
			led_set_solid(gps_g,0,0);
		}
		else {
			/* 6 steps flashing period */
			period=period_max-((period_max-period_min)*(st_count<6?st_count:6)/6);

			syslog(LOG_DEBUG,"[render] GPS search (satellite_count=%d,period=%d)",st_count,period);
			led_set_2timers(gps_r,gps_g,period,period);
		}
	}
	#endif	/* V_GPS_y */
	#else	/* V_LEDFUN_140 */
	#if defined(V_WIFI_backports)
	// WLAN led status
	if(DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(wireless_sta_radio) || DISP_ENT_DIRTY(wireless_ap_radio)) {
		int wirelessmodeap;
		int wirelessmodesta;
		int wlan_enable;

		wirelessmodesta=(DISP_ENT_INT(wireless_sta_radio)==1); // Wireless Mode AP
		wirelessmodeap=(DISP_ENT_INT(wireless_ap_radio)==1); // Wireless Mode STA
		wlan_enable=wirelessmodesta||wirelessmodeap;

	#else	/* V_WIFI_backports */
	// WLAN led status
	if( DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(wireless_mode) || DISP_ENT_DIRTY(wlan_enable)) {

		int wirelessmodeap;
		int wirelessmodesta;
		wirelessmodeap=(DISP_ENT_INT(wireless_mode)==RDB_VAL(WLAN_0_WIFI_MODE,AP)); // Wireless Mode AP
		wirelessmodesta=(DISP_ENT_INT(wireless_mode)==RDB_VAL(WLAN_0_WIFI_MODE,STA)); // Wireless Mode STA
	#endif	/* V_WIFI_backports */
		if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to wifi leds");

			led_set_solid(wlan_r,0,0);
			led_set_solid(wlan_g,0,0);
		}
		else if(!wlan_enable) {
			syslog(LOG_DEBUG,"[led-off] set wifi disabled leds");

			led_set_solid(wlan_r,0,0);
			led_set_solid(wlan_g,0,0);
		}
		else if(wirelessmodesta) {
			syslog(LOG_DEBUG,"[render] set wireless mode STA led");
			/* FIXME Long term fix is to modify cdcs trigger driver to support catering to multiple LED per trigger */
			led_set_traffic(wlan_g,0,"cdcs-wlanap",0,255);
			led_set_traffic(wlan_r,0,"cdcs-wlanst",0,255);
		}
		else if(wirelessmodeap) {
			syslog(LOG_DEBUG,"[render] set wireless mode AP led");

			led_set_solid(wlan_r,0,0);
			led_set_traffic(wlan_g,0,"cdcs-wlanap",0,255);
		}
		else {
			syslog(LOG_DEBUG,"[render] set wireless mode off");

			led_set_solid(wlan_r,0,0);
			led_set_solid(wlan_g,0,0);
		}
	}
	#endif	/* V_LEDFUN_140 */
			// Network led - wwan_online_status
	if(
	    DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(dim_screen) ||
	    DISP_ENT_DIRTY(sim_card_status) || DISP_ENT_DIRTY(module_name) ||
	    DISP_ENT_DIRTY(wwan_enable_status) || DISP_ENT_DIRTY(wwan_connecting_status) ||
	    DISP_ENT_DIRTY(wwan_online_status) || DISP_ENT_DIRTY(cdma_activated) ||
	    DISP_ENT_DIRTY(wwan_reg) || DISP_ENT_DIRTY(booting_delay) ||
	    DISP_ENT_DIRTY(factory_reset_mode)
	) {
		int online;
		int offline;
		int connecting;

		int reg;
		int registered;
		int registering;

		int sim_puk;
		int sim_pin;
		int sim_not_inserted;

		int module_ready;
		int not_activated;

		// apply all 3g hardware failure in production mode
		module_ready=*DISP_ENT_STR(module_name);

		// get online and connecting status
		online=DISP_ENT_INT(wwan_online_status)==1;
		offline=DISP_ENT_INT(wwan_online_status)==0;
		connecting=DISP_ENT_INT(wwan_connecting_status);

		// get cdma activation status
		not_activated=DISP_ENT_INT(cdma_activated)==0;

		// get network activity
		reg=atoi(DISP_ENT_STR(wwan_reg));
		registered=(reg==1) || (reg>=5);
		registering=(reg==2);

		// get sim status
		sim_puk=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_PUK));
		sim_pin=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_PIN));
		sim_not_inserted=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_NOT_INSERTED)) || (DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,NEGOTIATING));

		if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to wwan leds");
#if defined(V_LEDFUN_nwl22) || defined(V_LEDFUN_ntc_220)
			if(dimscreen_timer) {
				led_set_solid(wwan_r,0,0);
			}
			else {
				led_set_solid(wwan_r,0,255);
			}
#else
			led_set_solid(wwan_r,0,0);
#endif
			led_set_solid(wwan_g,0,0);
		}
		else if (in_factory_reset_mode) {
			led_set_solid(wwan_r,0,255);
			led_set_solid(wwan_g,0,0);
		}
		// Can't connect
		else if(sim_not_inserted && module_ready) {
			syslog(LOG_DEBUG,"[render] set sim-not-inserted led");

			led_set_solid(wwan_r,0,255);
			led_set_solid(wwan_g,0,0);
		}
		// SIM PUK locked
		else if(sim_puk && module_ready) {
			syslog(LOG_DEBUG,"[render] set SIM PUK led");

			led_set_timer(wwan_r,0,200,200);
			led_set_solid(wwan_g,0,0);

		}
		// SIM PIN locked
		else if(sim_pin && !DISP_ENT_INT(booting_delay) && module_ready) {
			syslog(LOG_DEBUG,"[render] set SIM PIN led");

			led_set_timer(wwan_r,0,2000,2000);
			led_set_solid(wwan_g,0,0);
		}
		// Connected and Traffic via wwan
		else if(online && module_ready) {
			syslog(LOG_DEBUG,"[render] set online led");

			led_set_solid(wwan_r,0,0);
			led_set_traffic(wwan_g,0,"cdcs-wwan",0,255);
		}
		// Connecting PDP
		else if((connecting==1) && module_ready) {
			syslog(LOG_DEBUG,"[render] set connecting led");

			led_set_solid(wwan_r,0,0);
			led_set_timer(wwan_g,0,2000,2000);
		}
		// Registered network
		else if(registered && !offline && module_ready) {
			syslog(LOG_DEBUG,"[render] set registered led");

			led_set_solid(wwan_r,0,255);
			led_set_solid(wwan_g,0,255);
		}
		// Registering network
		else if(!registered && module_ready) {
			syslog(LOG_DEBUG,"[render] set registering led");

			led_set_2timers(wwan_r,wwan_g,2000,2000);
		}
		else if(!DISP_ENT_INT(wwan_enable_status)) {
			syslog(LOG_DEBUG,"[render] set wwan disable led");
#if defined(V_LEDFUN_nwl22) || defined(V_LEDFUN_ntc_220)
			led_set_solid(wwan_r,0,255);
#else
			led_set_solid(wwan_r,0,0);
#endif
			led_set_solid(wwan_g,0,0);
		}
		// hold on the red led during the boot up period
		else if(DISP_ENT_INT(booting_delay)) {
                }
		else if(connecting==0) {
			syslog(LOG_DEBUG,"[render] set offline led");

			led_set_solid(wwan_r,0,255);
			led_set_solid(wwan_g,0,0);
		}
	}


	// rssi led - signal_strength & service_type
	if(DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(service_type) || DISP_ENT_DIRTY(signal_strength)) {
		int gsm;
		int gprs;
		int umts;
		int lte;
		int bars;
		int rssi_thresholds_wcdma[RSSI_LED_NUMBERS]={-109,-101,-91,-85,-77};
		int rssi_thresholds_gsm[RSSI_LED_NUMBERS]={-109,-102,-93,-87,-78};
#if defined(V_LEDFUN_nwl22) || defined(V_LEDFUN_ntc_220)
		int rsrp_thresholds_lte[RSSI_LED_NUMBERS]={-120,-100,-90,-80,-70};
#else
		int rsrp_thresholds_lte[RSSI_LED_NUMBERS]={-120,-115,-105,-100,-90};
#endif
		int sig_strength;
		int i;
		int r;
		int g;

		// use RAT (^SMONI for Cinterion)
		gprs=(DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,EGPRS)) // EGPRS
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GPRS)) // GPRS
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XRTT)) // CDMA 1x
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,DC_HSPAP)) // DC-HSPA+
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,HSPAP)); // HSPA+

		gsm=DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GSM)
				|| DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GSM_COMPACT)
				|| DISP_ENT_INT(service_type)==-1; // GSM

		lte=DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,LTE); // LTE
		umts=!gprs && !gsm && !lte;
		/* set evdo */
		umts=umts \
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XEVDO_RELEASE_0))
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XEVDO_RELEASE_A))
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA2000_1X))
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,UMTS));

		// get signal strength(rssi or rsrp)
		sig_strength=atoi(DISP_ENT_STR(signal_strength));
		// get bars
		bars=0;
		for(i=0;i<RSSI_LED_NUMBERS;i++) {
			if(gsm && sig_strength && (rssi_thresholds_gsm[i]<=sig_strength))
				bars++;
			else if((umts || gprs) && sig_strength && (rssi_thresholds_wcdma[i]<=sig_strength))
				bars++;
			else if(lte && sig_strength && (rsrp_thresholds_lte[i]<sig_strength))
				bars++;
		}

		syslog(LOG_DEBUG,"[disp] rssi=%d,service_type=%s,bars=%d,gsm=%d,grps=%d,umts=%d,lte=%d",sig_strength,DISP_ENT_STR(service_type),bars,gsm,gprs,umts,lte);

		if(DISP_ENT_INT(dim_screen))
			syslog(LOG_DEBUG,"[led-off] dim-mode to rssi leds");

		for(i = 3;i < 3 + RSSI_LED_NUMBERS;i++) {
			g=!DISP_ENT_INT(dim_screen) && ((i - 3) <bars) && (umts||gprs||lte);
			r=!DISP_ENT_INT(dim_screen) && ((i - 3) <bars) && (gsm||gprs||umts);

			syslog(LOG_DEBUG,"[disp] bar_index=%d, r=%d, g=%d",i - 3,r,g);

			led_set_solid("led%dr",i+1,r);
			led_set_solid("led%dg",i+1,g);
		}
	}

	return 0;

}
#endif

#if defined(V_LEDFUN_nwl22w) || defined(V_LEDFUN_145w)
#define WIFI_LED_ON_PERIOD_MS	"90"
#define WIFI_LED_OFF_PERIOD_MS	"90"

typedef enum {
	INIT_LED,
	OFF_LED,
	AP_NO_TRAFFIC,
	STAONLY_NO_TRAFFIC,
	AP_TRAFFIC,
	STAONLY_TRAFFIC,
} led_status;

/*
 * Initialise wifi LED
 */
static void init_wifi_led(void)
{
	led_put_value(wlan_g, "trigger", "cdcs-wlanap",0);
	led_put_value(wlan_r, "trigger", "cdcs-wlanst",0);
	led_put_value(wlan_g, "trigger_pulse", WIFI_LED_ON_PERIOD_MS,0);
	led_put_value(wlan_r, "trigger_pulse", WIFI_LED_ON_PERIOD_MS,0);
	led_put_value(wlan_g, "trigger_dead", WIFI_LED_OFF_PERIOD_MS,0);
	led_put_value(wlan_r, "trigger_dead", WIFI_LED_OFF_PERIOD_MS,0);
}

/*
 * turn off wifi LED
 */
static void turn_off_wifi_led(void)
{
	init_wifi_led();
	led_put_value(wlan_g, "force", "off",0);
	led_put_value(wlan_r, "force", "off",0);
}

/*
 * set WiFi LED
 * @status: status to which LED is set
 */
static void set_wifi_led(led_status status)
{
	static led_status cur_status = INIT_LED;

	if (cur_status == status) {
		return;
	}

	switch (status) {
		case OFF_LED:
			turn_off_wifi_led();
			break;
		case AP_NO_TRAFFIC:
			init_wifi_led();
			led_put_value(wlan_g, "force", "on",0);
			led_put_value(wlan_r, "force", "off",0);
			break;
		case STAONLY_NO_TRAFFIC:
			init_wifi_led();
			led_put_value(wlan_g, "force", "on",0);
			led_put_value(wlan_r, "force", "on",0);
			break;
		case AP_TRAFFIC:
			init_wifi_led();
			led_put_value(wlan_r, "force", "off",0);
			led_put_value(wlan_g, "force", "trigger_cont",0);
			break;
		case STAONLY_TRAFFIC:
			init_wifi_led();
			led_put_value(wlan_g, "force", "trigger_cont",0);
			led_put_value(wlan_r, "force", "trigger_cont",0);
			break;
		default:
			break;
	}

	cur_status = status;
}

/*
	render for kudu board like nwl22w - display disps (virtual LEDs - information) to physical (LEDs) for kudu board
*/
static int render_disps_kudu()
{
	int i;
	// process led dance
	i = process_led_dance();

	if(i == -1) {
		return i;
	}

    const bool in_factory_reset_mode = (
#if defined(V_REQUIRE_FACTORY_PASSWORD_CHANGE_y)
        DISP_ENT_INT(factory_reset_mode) == RDB_VAL(ADMIN_FACTORY_DEFAULT_PASSWORDS_IN_USE, 1)
#else
	0
#endif
    );

	// power led - router_power_status
	if(
	    DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(booting_delay) ||
	    DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(boot_mode) ||
	    DISP_ENT_DIRTY(sim_card_status) || DISP_ENT_DIRTY(module_name) ||
	    DISP_ENT_DIRTY(factory_reset_mode)
	) {

		int recovery;
		int sim_not_inserted;

		int hwf_module;

		recovery=*DISP_ENT_STR(boot_mode);

		// apply all 2g/3g/lte hardware failure in production mode
		sim_not_inserted=!recovery && (DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_NOT_INSERTED));
		hwf_module=!recovery && !DISP_ENT_INT(booting_delay) && !*DISP_ENT_STR(module_name);
		// powering up
		if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to power leds");

			led_set_solid(power_g,0,0);
			led_set_solid(power_r,0,0);
		}
		else if (in_factory_reset_mode) {
			syslog(LOG_DEBUG,"[led-off] factory default mode");
			led_set_solid(power_r,0,0);
			led_set_solid(power_g,0,255);
		}
		// Hardware error
		else if (sim_not_inserted || hwf_module) {

			if(sim_not_inserted)
				syslog(LOG_ERR,"[hardware failure] no sim card detected");
			if(hwf_module)
				syslog(LOG_ERR,"[hardware failure] no phone module detected");

			syslog(LOG_DEBUG,"[render] set hardware failure led - sim=%d,module=%d",sim_not_inserted,hwf_module);

			led_set_timer(power_r,0,2000,2000);
			led_set_solid(power_g,0,0);
		}
		// power on in recovery mode
		else if(recovery) {
			syslog(LOG_DEBUG,"[render] set power led (recovery=%d)",recovery);
			led_set_solid(power_r,0,255);
			led_set_solid(power_g,0,255);
		}
		// power on
		else {
			syslog(LOG_DEBUG,"[render] set power led ");
			led_set_solid(power_r,0,0);
			led_set_solid(power_g,0,255);
		}
	}

	// WLAN led status
	if(
		DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(dim_screen) ||
		DISP_ENT_DIRTY(wireless_sta_radio) || DISP_ENT_DIRTY(wireless_ap_radio) ||
		DISP_ENT_DIRTY(wireless_traffic) || DISP_ENT_DIRTY(factory_reset_mode)
	) {
		int wirelessmodeap;
		int wirelessmodesta;
		int wlan_enable;

		wirelessmodesta=(DISP_ENT_INT(wireless_sta_radio)==1); // Wireless Mode AP
		wirelessmodeap=(DISP_ENT_INT(wireless_ap_radio)==1); // Wireless Mode STA
		wlan_enable=wirelessmodesta||wirelessmodeap;

		if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to wifi leds");

			set_wifi_led(OFF_LED);
		}
		else if(!wlan_enable) {
			syslog(LOG_DEBUG,"[led-off] set wifi disabled leds");

			set_wifi_led(OFF_LED);
		}
		else {
			if (DISP_ENT_INT(wireless_traffic) == 1) {
				/* traffic flowing */
				if (wirelessmodeap) {
					set_wifi_led(AP_TRAFFIC);
				}
				else {
					set_wifi_led(STAONLY_TRAFFIC);
				}
			}
			else {
				/* no traffic flowing or init state or invalid data, consider it as no traffic flow */
				if (wirelessmodeap) {
					set_wifi_led(AP_NO_TRAFFIC);
				}
				else {
					set_wifi_led(STAONLY_NO_TRAFFIC);
				}
			}
		}
	}

	// Network led - wwan_online_status
	if(DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(sim_card_status) || DISP_ENT_DIRTY(module_name) || DISP_ENT_DIRTY(wwan_enable_status) || DISP_ENT_DIRTY(wwan_connecting_status) || DISP_ENT_DIRTY(wwan_online_status) || DISP_ENT_DIRTY(cdma_activated) || DISP_ENT_DIRTY(wwan_reg) || DISP_ENT_DIRTY(booting_delay)) {
		int online;
		int offline;
		int connecting;

		int reg;
		int registered;
		int registering;

		int sim_puk;
		int sim_pin;
		int sim_not_inserted;

		int module_ready;
		int not_activated;

		// apply all 3g hardware failure in production mode
		module_ready=*DISP_ENT_STR(module_name);

		// get online and connecting status
		online=DISP_ENT_INT(wwan_online_status)==1;
		offline=DISP_ENT_INT(wwan_online_status)==0;
		connecting=DISP_ENT_INT(wwan_connecting_status);

		// get cdma activation status
		not_activated=DISP_ENT_INT(cdma_activated)==0;

		// get network activity
		reg=atoi(DISP_ENT_STR(wwan_reg));
		registered=(reg==1) || (reg>=5);
		registering=(reg==2);

		// get sim status
		sim_puk=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_PUK));
		sim_pin=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_PIN));
		sim_not_inserted=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_NOT_INSERTED)) || (DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,NEGOTIATING));

		if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to wwan leds");

			if(dimscreen_timer) {
				led_set_solid(wwan_r,0,0);
			}
			else {
				led_set_solid(wwan_r,0,255);
			}

			led_set_solid(wwan_g,0,0);
		}
		// Can't connect or in factory default mode
		else if(in_factory_reset_mode || (sim_not_inserted && module_ready)) {
			syslog(LOG_DEBUG,"[render] set sim-not-inserted led");

			led_set_solid(wwan_r,0,255);
			led_set_solid(wwan_g,0,0);
		}
		// SIM PUK locked
		else if(sim_puk && module_ready) {
			syslog(LOG_DEBUG,"[render] set SIM PUK led");

			led_set_timer(wwan_r,0,200,200);
			led_set_solid(wwan_g,0,0);

		}
		// SIM PIN locked
		else if(sim_pin && !DISP_ENT_INT(booting_delay) && module_ready) {
			syslog(LOG_DEBUG,"[render] set SIM PIN led");

			led_set_timer(wwan_r,0,2000,2000);
			led_set_solid(wwan_g,0,0);
		}
		// Connected and Traffic via wwan
		else if(online && module_ready) {
			syslog(LOG_DEBUG,"[render] set online led");

			led_set_solid(wwan_r,0,0);
			led_set_traffic(wwan_g,0,"cdcs-wwan",0,255);
		}
		// Connecting PDP
		else if((connecting==1) && module_ready) {
			syslog(LOG_DEBUG,"[render] set connecting led");

			led_set_solid(wwan_r,0,0);
			led_set_timer(wwan_g,0,2000,2000);
		}
		// Registered network
		else if(registered && !offline && module_ready) {
			syslog(LOG_DEBUG,"[render] set registered led");

			led_set_solid(wwan_r,0,255);
			led_set_solid(wwan_g,0,255);
		}
		// Registering network
		else if(!registered && module_ready) {
			syslog(LOG_DEBUG,"[render] set registering led");

			led_set_2timers(wwan_r,wwan_g,2000,2000);
		}
		else if(!DISP_ENT_INT(wwan_enable_status)) {
			syslog(LOG_DEBUG,"[render] set wwan disable led");

			led_set_solid(wwan_r,0,255);

			led_set_solid(wwan_g,0,0);
		}
		// hold on the red led during the boot up period
		else if(DISP_ENT_INT(booting_delay)) {
		}
		else if(connecting==0) {
			syslog(LOG_DEBUG,"[render] set offline led");

			led_set_solid(wwan_r,0,255);
			led_set_solid(wwan_g,0,0);
		}
	}


	// rssi led - signal_strength & service_type
	if(DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(service_type) || DISP_ENT_DIRTY(signal_strength)) {
		int gsm;
		int gprs;
		int umts;
		int lte;
		int bars;
#if defined(V_SKIN_TEL)
		int rssi_thresholds_wcdma[RSSI_LED_NUMBERS]= {-108,-101,-92,-86,-77};
		int rssi_thresholds_gsm[RSSI_LED_NUMBERS]  = {-109,-101,-91,-85,-77};
		int rsrp_thresholds_lte[RSSI_LED_NUMBERS]  = {-120,-115,-105,-100,-90};
#else
		int rssi_thresholds_wcdma[RSSI_LED_NUMBERS]= {-109,-101,-91,-85,-77};
		int rssi_thresholds_gsm[RSSI_LED_NUMBERS]  = {-109,-101,-91,-85,-77};
		int rsrp_thresholds_lte[RSSI_LED_NUMBERS]  = {-120,-100,-90,-80,-70};
#endif

		int sig_strength;
		int i;
		int r;
		int g;

		// use RAT (^SMONI for Cinterion)
		gprs=(DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,EGPRS)) // EGPRS
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GPRS)) // GPRS
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XRTT)) // CDMA 1x
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,DC_HSPAP)) // DC-HSPA+
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,HSPAP)); // HSPA+

		gsm=DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GSM)
				|| DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GSM_COMPACT)
				|| DISP_ENT_INT(service_type)==-1; // GSM

		lte=DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,LTE); // LTE
		umts=!gprs && !gsm && !lte;
		/* set evdo */
		umts=umts \
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XEVDO_RELEASE_0))
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XEVDO_RELEASE_A))
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,UMTS));

		// get signal strength(rssi or rsrp)
		sig_strength=atoi(DISP_ENT_STR(signal_strength));
		// get bars
		bars=0;
		for(i=0;i<RSSI_LED_NUMBERS;i++) {
			if(gsm && sig_strength && (rssi_thresholds_gsm[i]<=sig_strength))
				bars++;
			else if((umts || gprs) && sig_strength && (rssi_thresholds_wcdma[i]<=sig_strength))
				bars++;
			else if(lte && sig_strength && (rsrp_thresholds_lte[i]<sig_strength))
				bars++;
		}

		syslog(LOG_DEBUG,"[disp] rssi=%d,service_type=%s,bars=%d,gsm=%d,grps=%d,umts=%d,lte=%d",sig_strength,DISP_ENT_STR(service_type),bars,gsm,gprs,umts,lte);

		if(DISP_ENT_INT(dim_screen))
			syslog(LOG_DEBUG,"[led-off] dim-mode to rssi leds");

		for(i = 0;i < RSSI_LED_NUMBERS;i++) {
			g=!DISP_ENT_INT(dim_screen) && (i <bars) && (umts||gprs||lte);
			r=!DISP_ENT_INT(dim_screen) && (i <bars) && (gsm||gprs||umts);

			syslog(LOG_DEBUG,"[disp] bar_index=%d, r=%d, g=%d",i,r,g);

			if(i == 0) {
				led_set_solid("led6r",0,r);
				led_set_solid("led6g",0,g);
			}
			else if(i == 1) {
				led_set_solid("led7r",0,r);
				led_set_solid("led7g",0,g);
			}
			else {
				led_set_solid("led%dr",i-1,r);
				led_set_solid("led%dg",i-1,g);
			}
		}

	}
	return 0;

}
#endif
#if defined(V_LEDFUN_falcon_default) || defined(V_LEDFUN_falcon_with_aux) || defined(V_LEDFUN_falcon_1101) || defined(V_LEDFUN_nmc1000) || defined(V_LEDFUN_ntc_70)
/*
	render for falcon board - display disps (virtual LEDs - information) to physical (LEDs) for Falcon board
*/
static int render_disps_falcon()
{
	int i;
#ifdef V_LEDFUN_falcon_1101
	int sim_led_blinking=0;
#endif

	// process led dance
	{
		int dancing;
		int newtype;
		static int type=-1;
		static int step=0;
		static int loop=0;

		static clock_t danceon_time=0;
		static enum led_dance_stage stage=led_dance_stage_set;

		// if pattern dance is triggered
		if(DISP_ENT_DIRTY(pattern_dance) || DISP_ENT_DIRTY(dispd_disable)) {
			newtype=DISP_ENT_INT(pattern_dance);

			if(newtype<0) {
				if(type>=0){
					syslog(LOG_INFO,"[led-pattern] stop a pattern");
				}
			}
			else {
				loop=0;
				step=0;
				stage=led_dance_stage_set;

				syslog(LOG_INFO,"[led-pattern] start a pattern - %s",DISP_ENT_STR(pattern_dance));
			}

			type=newtype;
		}

		// get dancing flag
		dancing=type>=0;

		// use intensive and aggressive select timeout for dancing pattern
		seltimeout=dancing?10:DEFAULT_SELTIMEOUT;

		// keep on dancing in dance mode
		if(dancing) {
			struct led_pattern_t* pats;

			pats=pats_info[type].pats;

			switch(stage) {
				case led_dance_stage_set:
					syslog(LOG_DEBUG,"[led-pattern] set pattern - type=%d,name=%s,time=%d,step=%d,loop=%d",type,pats[step].name,pats[step].ms,step,loop);
					render_led_pattern(leds,&pats[step]);

					danceon_time=cur;
					stage=led_dance_stage_wait;
					break;

				case led_dance_stage_wait:
					if(!pats[step].name)
						goto do_not_call_me_again;

					if(cur-danceon_time<pats[step].ms*TCK_PER_SEC/1000)
						break;

					step=step+1;

					// rewind if we are at the end
					if(!pats[step].name) {
						loop++;

						// we don't dance any more if it exceeds loop count
						// the loop continues forever when loop is zero.
						if(pats_info[type].loop && (loop>=pats_info[type].loop)) {
							syslog(LOG_INFO,"[led-pattern] loop pattern done");
							goto do_not_call_me_again;
						}

						step=0;
					}

					stage=led_dance_stage_set;

					syslog(LOG_DEBUG,"[led-pattern] move to the next step - step=%d",step);
					break;
			}

			goto call_me_again;
		}
	}

	// external overriding
	if(DISP_ENT_DIRTY(dispd_disable)) {
		if(DISP_ENT_INT(dispd_disable)==RDB_VAL(DISPD_DISABLE,1)) {

			syslog(LOG_INFO,"user LED overriding mode activated");

#if !defined(V_LEDFUN_falcon_1101)
			led_set_solid("power_r",0,0);
			led_set_solid("power_g",0,0);
#endif
#if !defined(V_LEDFUN_falcon_1101)
			led_set_solid("wwan_r",0,0);
#endif
			led_set_solid("wwan_g",0,0);

			for(i=0;i<RSSI_LED_NUMBERS;i++) {
				led_set_solid("rssi%d_g",i+1,0);
#if !defined (V_LEDFUN_falcon_1101)
				led_set_solid("rssi%d_r",i+1,0);
#endif
			}
		}
		else {
			syslog(LOG_INFO,"router normal LED mode activated");
		}

	}

	// bypass in user overriding mode
	if(DISP_ENT_INT(dispd_disable)==RDB_VAL(DISPD_DISABLE,1)) {

		seltimeout=DEFAULT_SELTIMEOUT;
		goto do_not_call_me_again;
	}

	#if defined(V_GPS_y) && !defined (V_LEDFUN_nmc1000)
	// aux led - satellite_data
	if(DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(pattern_dance) || DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(satellite_valid) || DISP_ENT_DIRTY(gps_enable) || DISP_ENT_DIRTY(satellite_count)) {

		/*
			0,07,18,07,305;0,16,31,45,056;0,20,18,74,272;0,23,24,N/A,N/A;0,31,30,16,133;0,32,27,62,032;0,06,N/A,N/A,N/A;0,17,N/A,N/A,N/A;0,19,N/A,N/A,N/A;0,N/A,N/A,N/A,N/A;0,N/A,N/A,N/A,N/A;0,N/A,N/A,N/A,N/A;
		*/

		int st_count;

		int period;
		const int period_max=1500;
		const int period_min=100;

		/* get satellite count */
		st_count=atoi(DISP_ENT_STR(satellite_count));

		if(0) {
		}
		/* dim screen */
		else if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to aux led");

			led_set_solid("aux_r",0,0);
			led_set_solid("aux_g",0,0);
		}
		/* gps disabled */
		else if(DISP_ENT_INT(gps_enable)<0 || DISP_ENT_INT(gps_enable)==RDB_VAL(SENSORS_GPS_0_ENABLE,0)) {
			syslog(LOG_DEBUG,"[render] GPS disabled");

			led_set_solid("aux_r",0,0);
			led_set_solid("aux_g",0,0);
		}
		/* gps aquired */
		else if(DISP_ENT_INT(satellite_valid)==RDB_VAL(SENSORS_GPS_0_STANDALONE_VALID,VALID)) {
			syslog(LOG_DEBUG,"[render] GPS acquired");

			led_set_solid("aux_r",0,0);
			led_set_solid("aux_g",0,255);
		}
		/* no satellite found */
		else if(st_count==0) {
			syslog(LOG_DEBUG,"[render] no satellite found");
			led_set_solid("aux_r",0,255);
			led_set_solid("aux_g",0,0);
		}
		else {
			/* 6 steps flashing period */
			period=period_max-((period_max-period_min)*(st_count<6?st_count:6)/6);

			syslog(LOG_DEBUG,"[render] GPS search (satellite_count=%d,period=%d)",st_count,period);
			led_set_2timers("aux_r","aux_g",period,period);
		}
	}
	#endif


	// power led - router_power_status
	if (DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(pattern_dance) ||
		DISP_ENT_DIRTY(booting_delay) || DISP_ENT_DIRTY(dim_screen) ||
		DISP_ENT_DIRTY(boot_mode)     || DISP_ENT_DIRTY(sim_card_status) ||
		DISP_ENT_DIRTY(module_name)   || DISP_ENT_DIRTY(wwan_enable_status)) {
		int recovery;
		int sim_not_inserted;

		int hwf_module;

		recovery=*DISP_ENT_STR(boot_mode);

		// apply all 3g hardware failure in production mode
		sim_not_inserted=!recovery && (DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_NOT_INSERTED));
		hwf_module=!recovery && !DISP_ENT_INT(booting_delay) && !*DISP_ENT_STR(module_name);

		if(0) {
		}
		else if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to power leds");

#if !defined(V_LEDFUN_falcon_1101)
			led_set_solid("power_r",0,0);
			led_set_solid("power_g",0,0);
#endif
		}
		else if ((sim_not_inserted
#if defined(V_LEDFUN_ntc_70)
			&& DISP_ENT_INT(wwan_enable_status)
#endif
				  )
				 || hwf_module) {

			if(sim_not_inserted)
				syslog(LOG_ERR,"[hardware failure] no sim card detected");
			if(hwf_module)
				syslog(LOG_ERR,"[hardware failure] no phone module detected");

			syslog(LOG_DEBUG,"[render] set hardware failure led - sim=%d,module=%d",sim_not_inserted,hwf_module);
#if !defined(V_LEDFUN_falcon_1101)
			led_set_timer("power_r",0,500,500);
			led_set_solid("power_g",0,0);
#endif
		}
#if !defined(V_LEDFUN_falcon_1101)
		else {
			syslog(LOG_DEBUG,"[render] set power led (recovery=%d)",recovery);
			led_set_solid("power_r",0,recovery?255:0);
			led_set_solid("power_g",0,255);
		}
#endif
	}

	// 3g led - wwan_online_status
	if(DISP_ENT_DIRTY(cdma_activated) || DISP_ENT_DIRTY(pppoe_en) || DISP_ENT_DIRTY(pppoe_stat) || DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(pattern_dance) || DISP_ENT_DIRTY(booting_delay) || DISP_ENT_DIRTY(dim_screen) || DISP_ENT_DIRTY(wwan_enable_status) || DISP_ENT_DIRTY(wwan_online_status) || DISP_ENT_DIRTY(wwan_connecting_status) || DISP_ENT_DIRTY(wwan_reg) || DISP_ENT_DIRTY(sim_card_status) || DISP_ENT_DIRTY(module_name)) {
		int online;
		int offline;
		int connecting;

		int reg;
		int registered;
		int registering;

		int sim_puk;
		int sim_pin;
		int sim_not_inserted;

		int module_ready;

		int pppoe_blinking;
		int pppoe_enable;
		int pppoe_online;

		int not_activated;

		// apply all 3g hardware failure in production mode
		module_ready=*DISP_ENT_STR(module_name);

		// get online and connecting status
		online=DISP_ENT_INT(wwan_online_status)==1;
		offline=DISP_ENT_INT(wwan_online_status)==0;
		connecting=DISP_ENT_INT(wwan_connecting_status);

		// get cdma activation status
		not_activated=DISP_ENT_INT(cdma_activated)==0;

		// pppoe status
		pppoe_enable=DISP_ENT_INT(pppoe_en)==1;
		pppoe_online=DISP_ENT_INT(pppoe_stat)==RDB_VAL(PPPOE_SERVER_0_STATUS,ONLINE);
		pppoe_blinking=DISP_ENT_INT(pppoe_stat)==RDB_VAL(PPPOE_SERVER_0_STATUS,IDLE) || DISP_ENT_INT(pppoe_stat)==RDB_VAL(PPPOE_SERVER_0_STATUS,DISCOVERY);

		// get network activity
		reg=atoi(DISP_ENT_STR(wwan_reg));
		registered=(reg==1) || (reg>=5);
		registering=(reg==2);

		// get sim status
		sim_puk=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_PUK));
		sim_pin=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_PIN));
		sim_not_inserted=(DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,SIM_NOT_INSERTED)) || (DISP_ENT_INT(sim_card_status)==RDB_VAL(WWAN_0_SIM_STATUS_STATUS,NEGOTIATING));

		if(0) {
		}
#if defined(V_LEDFUN_ntc_70)
		else if(!DISP_ENT_INT(wwan_enable_status)) {
			syslog(LOG_DEBUG, "[render] set no wwan profiles enabled led");
			led_set_solid("wwan_r", 0, 0);
			led_set_solid("wwan_g", 0, 0);
		}
#endif
		else if(DISP_ENT_INT(dim_screen)) {
			syslog(LOG_DEBUG,"[led-off] dim-mode to wwan leds");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_solid("wwan_r",0,0);
#endif
			led_set_solid("wwan_g",0,0);
		}
		else if(sim_not_inserted && module_ready) {
			syslog(LOG_DEBUG,"[render] set sim-not-inserted led");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_solid("wwan_r",0,255);
#endif
			led_set_solid("wwan_g",0,0);
		}
		else if(sim_puk && module_ready) {
			syslog(LOG_DEBUG,"[render] set SIM PUK led");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_timer("wwan_r",0,200,200);
#endif
			led_set_solid("wwan_g",0,0);
#ifdef V_LEDFUN_falcon_1101
			led_set_timer("rssi1_g",0,200,200);
			led_set_timer("rssi2_g",0,200,200);
			sim_led_blinking = 1;
#endif
		}

		else if(not_activated && module_ready) {
			syslog(LOG_DEBUG,"[render] set CDMA-not-activated led");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_timer("wwan_r",0,1000,1000);
#endif
			led_set_solid("wwan_g",0,0);
		}
		else if(sim_pin && !DISP_ENT_INT(booting_delay) && module_ready) {
			syslog(LOG_DEBUG,"[render] set SIM PIN led");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_timer("wwan_r",0,1000,1000);
#endif
			led_set_solid("wwan_g",0,0);
#ifdef V_LEDFUN_falcon_1101
			led_set_timer("rssi1_g",0,500,500);
			led_set_timer("rssi2_g",0,500,500);
			sim_led_blinking = 1;
#endif
		}
		else if(!pppoe_enable && online && module_ready) {
			syslog(LOG_DEBUG,"[render] set online led");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_solid("wwan_r",0,0);
#endif
			led_set_traffic("wwan_g",0,"cdcs-wwan",0,255);
		}
		else if(!pppoe_enable && (connecting==1) && module_ready) {
			syslog(LOG_DEBUG,"[render] set connecting led");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_solid("wwan_r",0,0);
#endif
			led_set_timer("wwan_g",0,1000,1000);
		}
		// if pppoe idle or disconvery
		else if(pppoe_enable && pppoe_blinking) {
			syslog(LOG_DEBUG,"[render] set pppoe blinking led");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_solid("wwan_r",0,0);
#endif
			led_set_timer("wwan_g",0,1000,1000);
		}
		// if pppoe online
		else if(pppoe_enable && pppoe_online) {
			syslog(LOG_DEBUG,"[render] set pppoe online led");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_solid("wwan_r",0,0);
#endif
			led_set_solid("wwan_g",0,255);
		}
		else if(registered && !offline && module_ready) {
			syslog(LOG_DEBUG,"[render] set registered led");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_solid("wwan_r",0,255);
#endif
			led_set_solid("wwan_g",0,255);
		}
		else if(registering && module_ready) {
			syslog(LOG_DEBUG,"[render] set registering led");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_2timers("wwan_r","wwan_g",500,500);
#endif
		}
		// hold on the red led during the boot up period
		else if(DISP_ENT_INT(booting_delay)) {
		}
		else {
			syslog(LOG_DEBUG,"[render] set offline led");
#if !defined(V_LEDFUN_falcon_1101)
			led_set_solid("wwan_r",0,255);
#endif
			led_set_solid("wwan_g",0,0);
		}
	}

#ifdef V_LEDFUN_falcon_1101
	if (sim_led_blinking)
		goto do_not_call_me_again;
#endif

#if ! defined(V_LEDFUN_ntc_70)

	// rssi led - signal_strength & service_type
	if(DISP_ENT_DIRTY(dispd_disable) 	||
	   DISP_ENT_DIRTY(pattern_dance) 	||
	   DISP_ENT_DIRTY(booting_delay) 	||
	   DISP_ENT_DIRTY(dim_screen)    	||
	   DISP_ENT_DIRTY(signal_strength) 	||
#ifdef V_LEDFUN_falcon_1101
	   sim_led_blinking == 0			||
#endif

	   DISP_ENT_DIRTY(service_type) ) {

		int gsm;
		int gprs;
		int umts;
		int lte;

		int bars;
#ifdef V_LEDFUN_falcon_1101
		int rssi_thresholds[RSSI_LED_NUMBERS]={-91,-77};
#else
		int rssi_thresholds[RSSI_LED_NUMBERS]={-109,-101,-91,-85,-77};
#endif
		int rsrp_thresholds_lte[RSSI_LED_NUMBERS]={-120,-115,-105,-100,-90};

		int sig_strength;
		int i;

		int r;
		int g;


		// use RAT (^SMONI for Cinterion)
		gprs=(DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,EGPRS)) // EGPRS
				|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GPRS)) // GPRS
				|| DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XRTT); // CDMA 1x

		gsm=DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GSM)
				|| DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,GSM_COMPACT)
				|| DISP_ENT_INT(service_type)==-1; // GSM

		lte=DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,LTE); // LTE

		umts=!gprs && !gsm && !lte;

		/* set evdo */
		umts=umts \
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XEVDO_RELEASE_0))
			|| (DISP_ENT_INT(service_type)==RDB_VAL(WWAN_0_SYSTEM_NETWORK_STATUS_SERVICE_TYPE,CDMA_1XEVDO_RELEASE_A));

		// get sig_strength
		sig_strength=atoi(DISP_ENT_STR(signal_strength));
		// get bars
		bars=0;
		for(i=0;i<RSSI_LED_NUMBERS;i++) {
			if(lte && sig_strength && (rsrp_thresholds_lte[i]<sig_strength))
				bars++;
			else if(sig_strength && (rssi_thresholds[i]<=sig_strength))
				bars++;
		}

		syslog(LOG_DEBUG,"[disp] sig_strength=%d,service_type=%s,bars=%d,gsm=%d,grps=%d,umts=%d,lte=%d",sig_strength,DISP_ENT_STR(service_type),bars,gsm,gprs,umts,lte);

		if(DISP_ENT_INT(dim_screen))
			syslog(LOG_DEBUG,"[led-off] dim-mode to sig_strength leds");

#ifdef V_LEDFUN_falcon_1101
		if (DISP_ENT_INT(dim_screen) || bars == 0) {
			led_set_solid("rssi1_g",0,0);
			led_set_solid("rssi2_g",0,0);
		} else if (bars == 1) {
			led_set_solid("rssi1_g",0,255);
			led_set_solid("rssi2_g",0,0);
		} else {
			led_set_solid("rssi1_g",0,255);
			led_set_solid("rssi2_g",0,255);
		}
#elif defined(V_LEDFUN_nmc1000)
		// only one rssi led which changes red <--> amber <--> green
		if (DISP_ENT_INT(dim_screen)) {
			led_set_solid("rssi1_g",0,0);
			led_set_solid("rssi1_r",0,0);
		} else if (bars == 0) {
			led_set_solid("rssi1_g",0,0);
			led_set_solid("rssi1_r",0,255);
		} else if (bars < 3) {
			led_set_solid("rssi1_g",0,255);
			led_set_solid("rssi1_r",0,255);
		} else {
			led_set_solid("rssi1_g",0,255);
			led_set_solid("rssi1_r",0,0);
		}

		// expasion led : network technology
		// green : 3G
		// amber : 2G GPRS
		// red   : GSM only (no GPRS)
		// off   : no network
		if (DISP_ENT_INT(dim_screen)) {
			led_set_solid("exp_g",0,0);
			led_set_solid("exp_r",0,0);
		} else if (lte || umts) {
			led_set_solid("exp_g",0,255);
			led_set_solid("exp_r",0,0);
		} else if (gprs) {
			led_set_solid("exp_g",0,255);
			led_set_solid("exp_r",0,255);
		} else if (!gprs && gsm) {
			led_set_solid("exp_g",0,0);
			led_set_solid("exp_r",0,255);
		} else {
			led_set_solid("exp_g",0,0);
			led_set_solid("exp_r",0,0);
		}

#else
		for(i=0;i<RSSI_LED_NUMBERS;i++) {
			g=!DISP_ENT_INT(dim_screen) && (i<bars) && (umts||gprs||lte);
			r=!DISP_ENT_INT(dim_screen) && (i<bars) && (gsm||gprs);
			led_set_solid("rssi%d_r",i+1,r);
			led_set_solid("rssi%d_g",i+1,g);
		}
#endif
	}
#endif // ! defined(V_LEDFUN_ntc_70)

#if defined(V_LEDFUN_ntc_70)
	// Control wlan green led according to radio on/off state.
	if (DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(dim_screen) ||
		DISP_ENT_DIRTY(wireless_ap_radio)) {
		led_set_solid("wlan_g", 0, (DISP_ENT_INT(wireless_ap_radio) == 1) ? 255: 0);
	}

	// Control ZigBee green led according to radio on/off state, and pulse it off for activity.
	if (DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(dim_screen) ||
		DISP_ENT_DIRTY(zigbee_radio)) {
		if (DISP_ENT_INT(zigbee_radio) == 1) {
			led_set_solid("zigbee_g", 0, 255);
			led_set_traffic("zigbee_g", 0, "cdcs-zigbee", 0, 255);
		} else {
			led_set_solid("zigbee_g", 0, 0);
		}
	}
#endif

do_not_call_me_again:
	return 0;

call_me_again:
	return -1;
}
#endif /* #if defined(V_LEDFUN_falcon_default) || defined(V_LEDFUN_falcon_with_aux) || defined(V_LEDFUN_falcon_1101) */

#if defined(V_LEDFUN_ntc_20)

/*
 * Defines used to set up a cdcs-beat trigger with a heartbeat.
 * Repeating: double pulse followed by a rest period.
 */
#define HEARTBEAT_PATTERN "101"
#define HEARTBEAT_TEMPO_MSEC 100
#define HEARTBEAT_REST_MSEC 4700

static int render_disps_ntc_20 (void)
{
    int ix;

    /*
     * Record the last led beat that was set. This is needed to prevent
     * setting the same beat again if it is already set. Because that
     * would prematurely reset the beat.
     */
    static enum {
        BEAT_NONE,
        BEAT_RECOVERABLE_ERR,
        BEAT_SCREEN_OFF,
    } beat_set = BEAT_NONE;

    /* external overriding */
	if (DISP_ENT_DIRTY(dispd_disable)) {
		if (DISP_ENT_INT(dispd_disable) == RDB_VAL(DISPD_DISABLE, 1)) {
			syslog(LOG_INFO,"user LED overriding mode activated");
			led_set_solid("power_r", 0, LED_BRIGHTNESS_OFF);
			led_set_solid("power_g", 0, LED_BRIGHTNESS_OFF);
            beat_set = BEAT_NONE;
		}
		else {
			syslog(LOG_INFO,"router normal LED mode activated");
		}
	}

	/* bypass in user overriding mode */
	if (DISP_ENT_INT(dispd_disable) == RDB_VAL(DISPD_DISABLE, 1)) {
		seltimeout = DEFAULT_SELTIMEOUT;
        return 0;
	}

    /* process led dance */
    ix = process_led_dance();
    if (ix == -1) {
        /*
         * -1 means a pattern dance is in progress. Pattern dancing
         * always overrides regular led programming so return here.
         */
        beat_set = BEAT_NONE;
        return -1;
    }

    /* power led - router_power_status */
	if (DISP_ENT_DIRTY(dispd_disable) || DISP_ENT_DIRTY(pattern_dance) ||
		DISP_ENT_DIRTY(booting_delay) || DISP_ENT_DIRTY(dim_screen) ||
		DISP_ENT_DIRTY(boot_mode)     || DISP_ENT_DIRTY(sim_card_status) ||
		DISP_ENT_DIRTY(module_name)   || DISP_ENT_DIRTY(wwan_enable_status) ||
        DISP_ENT_DIRTY(battery_online) || DISP_ENT_DIRTY(battery_status) ||
        DISP_ENT_DIRTY(battery_percent)) {

		int recovery = *DISP_ENT_STR(boot_mode);
		int sim_not_inserted =
            !recovery && (DISP_ENT_INT(sim_card_status) ==
                          RDB_VAL(WWAN_0_SIM_STATUS_STATUS, SIM_NOT_INSERTED));
        bool battery_low = is_battery_low();
        int hwf_module = !recovery && !*DISP_ENT_STR(module_name);

        if (DISP_ENT_INT(booting_delay)) {
            /* Still booting - keep LED booting pattern */
		} else if (hwf_module) {
            /* Fatal (e.g. HW) error conditions. */

            syslog(LOG_ERR, "[fatal error] no phone module detected");

            led_set_solid("power_r", 0, LED_BRIGHTNESS_FULL_ON);
            led_set_solid("power_g", 0, LED_BRIGHTNESS_OFF);
            beat_set = BEAT_NONE;

        } else if (sim_not_inserted || battery_low) {
            /* Recoverable error conditions. */
            if (beat_set != BEAT_RECOVERABLE_ERR) {
                if (sim_not_inserted) {
                    syslog(LOG_ERR, "[recoverable error] no sim card detected");
                }
                if (battery_low) {
                    syslog(LOG_ERR, "[recoverable error] battery low");
                }

                led_set_beat("power_r", 0, LED_BRIGHTNESS_FULL_ON,
                             LED_BRIGHTNESS_OFF, HEARTBEAT_PATTERN,
                             HEARTBEAT_TEMPO_MSEC, HEARTBEAT_REST_MSEC);
                led_set_solid("power_g", 0, LED_BRIGHTNESS_OFF);
                beat_set = BEAT_RECOVERABLE_ERR;
            }
        } else if (DISP_ENT_INT(dim_screen)) {
            /* Screen off - initiate heartbeat pattern. */
            if (beat_set != BEAT_SCREEN_OFF) {
                syslog(LOG_DEBUG,"[led-screen-off] dim-mode to power led");
                led_set_solid("power_r", 0, LED_BRIGHTNESS_OFF);
                led_set_beat("power_g", 0, LED_BRIGHTNESS_FULL_ON,
                             LED_BRIGHTNESS_OFF, HEARTBEAT_PATTERN,
                             HEARTBEAT_TEMPO_MSEC, HEARTBEAT_REST_MSEC);
                beat_set = BEAT_SCREEN_OFF;
            }
        } else {
            /* Screen on and no other prevailing conditions - turn off LED. */
            syslog(LOG_DEBUG,"[led-screen-on] turn off power led");
            led_set_solid("power_r", 0, LED_BRIGHTNESS_OFF);
            led_set_solid("power_g", 0, LED_BRIGHTNESS_OFF);
            beat_set = BEAT_NONE;
        }
    }

    return 0;
}

#endif /* #if defined(V_LEDFUN_ntc_20) */

// change integer value of a disp entity and make render surface dirty
static void set_disp_ent_int(enum disp_idx_t idx,int val)
{
	DISP_ENT_INT(idx)=val;

	dirty_to_render=DISP_ENT_DIRTY(idx)=1;
}

// change string value of a disp entity and make render surface dirty
static void set_disp_ent_str(enum disp_idx_t idx,const char* str)
{
	strncpy(DISP_ENT_STR(idx),str,sizeof(DISP_ENT_STR(idx)));
	DISP_ENT_STR(idx)[sizeof(DISP_ENT_STR(idx))-1]=0;

	dirty_to_render=DISP_ENT_DIRTY(idx)=1;
}

static void render_make_all_dirty()
{
	int i;

	// make all disps dirty so that each platform render function can re-draw all the disps for the first time
	for(i=0;i<last_disp_ent;i++)
		DISP_ENT_DIRTY(i)=1;

	// clear global dirty flag
	dirty_to_render=1;
}

static void render_init()
{
	render_make_all_dirty();
}

static void render_disps()
{
	int i;
	int stat;

	if(!dirty_to_render)
		return;

	#if defined(V_LEDFUN_falcon_default) || defined(V_LEDFUN_falcon_with_aux) || defined(V_LEDFUN_falcon_1101) || defined(V_LEDFUN_nmc1000) || defined(V_LEDFUN_ntc_70)
	stat=render_disps_falcon();
	#elif defined(V_LEDFUN_140wx) || defined(V_LEDFUN_140) || defined(V_LEDFUN_nwl22) || defined(V_LEDFUN_ntc_220)
	stat=render_disps_nguni();
	#elif defined(V_LEDFUN_nwl22w) || defined(V_LEDFUN_145w)
	stat=render_disps_kudu();
	#elif defined(V_LEDFUN_wntd_idu)
	stat=render_disps_wntd_idu();
    #elif defined(V_LEDFUN_ntc_20)
    stat = render_disps_ntc_20();
	#else
	#error LED render of the board is not defined!
	#endif

	// clear individual dirty flags
	for(i=0;i<last_disp_ent;i++)
		DISP_ENT_DIRTY(i)=0;

	if(stat==0) {
		// clear global dirty flag
		dirty_to_render=0;
	}
}

int search_validx(const char* rdb,const char* val)
{
	char rdbval[RDB_VARIABLE_NAME_MAX_LEN+RDB_VARIABLE_MAX_LEN];

	if(!val || !*val)
		return -1;

	snprintf(rdbval,sizeof(rdbval),"%s:%s",rdb,val);
	return stridx_find(si_validx,rdbval);
}

int get_rdb_validx(const char* rdb)
{
	const char* val;

	if(!(val=rdb_getVal(rdb)))
		goto err;

	return search_validx(rdb,val);

err:
	return -1;
}

/*
	convert rdb event to display entry
*/
int parse_rdb(const char* rdb,const char* val,long* ret_varidx,long* ret_validx,enum disp_idx_t* ret_dispidx)
{
	long varidx;
	long validx;
	enum disp_idx_t dispidx;

	varidx=-1;
	validx=-1;
	dispidx=-1;

	syslog(LOG_DEBUG,"[rdb] trigged - rdb=%s",rdb);

	// get varidx
	if( (varidx=stridx_find(si_varidx,rdb))<0 ) {
		syslog(LOG_ERR,"non-subscribed rdb variable triggered - %s",rdb);
		goto err;
	}

	// validation check of varidx range
	if(varidx<0 || varidx>=RDB_GEN_COUNT) {
		syslog(LOG_ERR,"out of range varidx=%ld",varidx);
		goto err;
	}

	// get validx
	validx=search_validx(rdb,val);


	// get disp idx
	dispidx=varidx_to_dispidx[varidx];

	// return
	if(ret_varidx)
		*ret_varidx=varidx;
	if(ret_validx)
		*ret_validx=validx;
	if(ret_dispidx)
		*ret_dispidx=dispidx;

	return 0;
err:
	return -1;
}

static int copy_rdb_to_disp(const char* rdb,const char* val,long varidx,long validx,const enum disp_idx_t dispidx)
{
	int chg;
	const char* str;

	if(dispidx<0 || last_disp_ent<=dispidx) {
		syslog(LOG_ERR,"dispidx out of range (dipsidx=%d)",dispidx);
		goto err;
	}

	// if changed?
	chg=(val && strncmp(DISP_ENT_STR(dispidx),val,sizeof(DISP_ENT_STR(dispidx)))) || (DISP_ENT_INT(dispidx)!=validx);

	// if changed
	if(chg) {
		str=(val==NULL)?"":val;

		syslog(LOG_DEBUG,"[disp] disp changed - dispidx=%d,rdb=%s,val=('%s'==>'%s'),idx=(%ld==>%ld)",dispidx,rdb,DISP_ENT_STR(dispidx),str,DISP_ENT_INT(dispidx),validx);

		// copy string
		set_disp_ent_str(dispidx,str);
		set_disp_ent_int(dispidx,validx);

	}

	return 0;

err:
	return -1;
}

static int set_disp_ent_int_when_chg(const char* rdb,enum disp_idx_t dispidx,long new)
{
	if( new!=DISP_ENT_INT(dispidx) ) {
		if(verbosity)
			syslog(LOG_DEBUG,"[disp] disp changed #2 - dispidx=%d,rdb=%s,idx=(%ld==>%ld)",dispidx,rdb,DISP_ENT_INT(dispidx),new);

		set_disp_ent_int(dispidx,new);
	}

	return 0;
}

static int convert_rdb_to_disp(const char* rdb,const char* val,long varidx,long validx,const enum disp_idx_t dispidx)
{
	int new;

	switch(varidx) {

		case RDB_DISPD_USRLED_TOUCH:
		{
			syslog(LOG_INFO,"force to redraw all LEDs");

			render_make_all_dirty();
			break;
		}

		case RDB_DISPD_PATTERN_TYPE:
		{
			break;
		}

		// reset led off timer
		case RDB_SYSTEM_RESET_LED_OFF_TIMER:
		{
			syslog(LOG_INFO,"[led-off] resetting dim timer (sec) - current timer=%ld",dimscreen_timer);

			// update ledon time
			screenon_time=now;

			set_disp_ent_int(dim_screen,0);
			break;
		}

		case RDB_SYSTEM_LED_OFF_TIMER:
		{
#if defined(V_LEDFUN_ntc_20)
            /* Platforms with rdb value specified in seconds */
            dimscreen_timer=atoi(val);
#else
            /* Platforms with rdb value specified in minutes */
			dimscreen_timer=atoi(val)*60;
#endif

			syslog(LOG_INFO,"[led-off] set dim timer - timer=%ld sec",dimscreen_timer);

			screenon_time=now;

			set_disp_ent_int(dim_screen,0);
			break;
		}

		// wlan_connecting_status
		#ifdef V_WIFI_none
		// no wlan
		#else
		case RDB_WLAN_0_ENABLE:
		case RDB_WLAN_1_ENABLE:
		case RDB_WLAN_2_ENABLE:
		case RDB_WLAN_3_ENABLE: {
			int pf1=get_rdb_validx("wlan.0.enable");
			int pf2=get_rdb_validx("wlan.1.enable");
			int pf3=get_rdb_validx("wlan.2.enable");
			int pf4=get_rdb_validx("wlan.3.enable");
			int pf5=get_rdb_validx("wlan.4.enable");

			new=pf1==1 || pf2==1 || pf3==1 || pf4==1 || pf5==1;
			set_disp_ent_int_when_chg(rdb,wlan_enable,new);

			break;
		}
		#endif

		// wwan_connecting_status
		case RDB_LINK_PROFILE_1_CONNECTING:
		#ifdef V_MULTIPLE_WWAN_PROFILES_y
		case RDB_LINK_PROFILE_2_CONNECTING:
		case RDB_LINK_PROFILE_3_CONNECTING:
		case RDB_LINK_PROFILE_4_CONNECTING:
		case RDB_LINK_PROFILE_5_CONNECTING:
		case RDB_LINK_PROFILE_6_CONNECTING:
		#endif
		{
			int pf1=get_rdb_validx("link.profile.1.connecting");
			#ifdef V_MULTIPLE_WWAN_PROFILES_y
			int pf2=get_rdb_validx("link.profile.2.connecting");
			int pf3=get_rdb_validx("link.profile.3.connecting");
			int pf4=get_rdb_validx("link.profile.4.connecting");
			int pf5=get_rdb_validx("link.profile.5.connecting");
			int pf6=get_rdb_validx("link.profile.6.connecting");
			#endif

			// if any of profile is connecting
			if( pf1==RDB_VAL(LINK_PROFILE_1_CONNECTING,1)
			#ifdef V_MULTIPLE_WWAN_PROFILES_y
				|| pf2==RDB_VAL(LINK_PROFILE_1_CONNECTING,1) || pf3==RDB_VAL(LINK_PROFILE_1_CONNECTING,1) || pf4==RDB_VAL(LINK_PROFILE_1_CONNECTING,1) || pf5==RDB_VAL(LINK_PROFILE_1_CONNECTING,1) || pf6==RDB_VAL(LINK_PROFILE_1_CONNECTING,1)
			#endif
			) {
				new=1;
			}
			else if( pf1==RDB_VAL(LINK_PROFILE_1_CONNECTING,0)
			#ifdef V_MULTIPLE_WWAN_PROFILES_y
				|| pf2==RDB_VAL(LINK_PROFILE_1_CONNECTING,0) || pf3==RDB_VAL(LINK_PROFILE_1_CONNECTING,0) || pf4==RDB_VAL(LINK_PROFILE_1_CONNECTING,0) || pf5==RDB_VAL(LINK_PROFILE_1_CONNECTING,0) || pf6==RDB_VAL(LINK_PROFILE_1_CONNECTING,0)
			#endif
			) {
				new=0;
			}
			else {
				new=-1;
			}

			set_disp_ent_int_when_chg(rdb,wwan_connecting_status,new);
			break;
			}

		case RDB_LINK_PROFILE_1_ENABLE:
		#ifdef V_MULTIPLE_WWAN_PROFILES_y
		case RDB_LINK_PROFILE_2_ENABLE:
		case RDB_LINK_PROFILE_3_ENABLE:
		case RDB_LINK_PROFILE_4_ENABLE:
		case RDB_LINK_PROFILE_5_ENABLE:
		case RDB_LINK_PROFILE_6_ENABLE:
		#endif
		{
			int enable;
			int pf1=get_rdb_validx("link.profile.1.enable");
			#ifdef V_MULTIPLE_WWAN_PROFILES_y
			int pf2=get_rdb_validx("link.profile.2.enable");
			int pf3=get_rdb_validx("link.profile.3.enable");
			int pf4=get_rdb_validx("link.profile.4.enable");
			int pf5=get_rdb_validx("link.profile.5.enable");
			int pf6=get_rdb_validx("link.profile.6.enable");
			#endif

			#ifdef V_MULTIPLE_WWAN_PROFILES_y
			enable=pf1==1|| pf2==1 || pf3==1 || pf4==1 || pf5==1  || pf6==1;
			#else
			enable=pf1==1;
			#endif

			new=enable?1:0;

			set_disp_ent_int_when_chg(rdb,wwan_enable_status,new);
			break;
		}

		// wwan_online_status
		case RDB_LINK_PROFILE_1_STATUS:
		#ifdef V_MULTIPLE_WWAN_PROFILES_y
		case RDB_LINK_PROFILE_2_STATUS:
		case RDB_LINK_PROFILE_3_STATUS:
		case RDB_LINK_PROFILE_4_STATUS:
		case RDB_LINK_PROFILE_5_STATUS:
		case RDB_LINK_PROFILE_6_STATUS:
		#endif
		{
			int pf1=get_rdb_validx("link.profile.1.status");
			#ifdef V_MULTIPLE_WWAN_PROFILES_y
			int pf2=get_rdb_validx("link.profile.2.status");
			int pf3=get_rdb_validx("link.profile.3.status");
			int pf4=get_rdb_validx("link.profile.4.status");
			int pf5=get_rdb_validx("link.profile.5.status");
			int pf6=get_rdb_validx("link.profile.6.status");
			#endif

			if( pf1==RDB_VAL(LINK_PROFILE_1_STATUS,UP)
			#ifdef V_MULTIPLE_WWAN_PROFILES_y
				|| pf2==RDB_VAL(LINK_PROFILE_1_STATUS,UP) || pf3==RDB_VAL(LINK_PROFILE_1_STATUS,UP) || pf4==RDB_VAL(LINK_PROFILE_1_STATUS,UP) || pf5==RDB_VAL(LINK_PROFILE_1_STATUS,UP)  || pf6==RDB_VAL(LINK_PROFILE_1_STATUS,UP)
			#endif
			) {
				new=1;
			}
			else if(pf1==RDB_VAL(LINK_PROFILE_1_STATUS,DOWN)
			#ifdef V_MULTIPLE_WWAN_PROFILES_y
				|| pf2==RDB_VAL(LINK_PROFILE_1_STATUS,DOWN) || pf3==RDB_VAL(LINK_PROFILE_1_STATUS,DOWN) || pf4==RDB_VAL(LINK_PROFILE_1_STATUS,DOWN) || pf5==RDB_VAL(LINK_PROFILE_1_STATUS,DOWN) || pf6==RDB_VAL(LINK_PROFILE_1_STATUS,DOWN)
			#endif
			) {
				new=0;
			}
			else {
				new=-1;
			}

			set_disp_ent_int_when_chg(rdb,wwan_online_status,new);
			break;
		}

		default:
			break;
	}


	return 0;
}

int init()
{
	const struct str_to_idx_t* p;

	int prior;
	pid_t pid;

	openlog("dispd",LOG_PID,LOG_USER);

	if(verbosity)
		setlogmask(LOG_UPTO(LOG_DEBUG));
	else
		setlogmask(LOG_UPTO(LOG_INFO));


	syslog(LOG_ERR,"=================");
	syslog(LOG_ERR,"loglevel check");
	syslog(LOG_ERR,"=================");
	syslog(LOG_ERR,"syslog LOG_ERR");
	syslog(LOG_INFO,"syslog LOG_INFO");
	syslog(LOG_DEBUG,"syslog LOG_DEBUG");

	memset(disp,sizeof(disp),0);

	// ignore signals
	signal(SIGCHLD, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);

	/* set highiest priority */
	pid=getpid();
	//prior=getpriority(PRIO_PROCESS,pid);
	prior=-20;
	setpriority(PRIO_PROCESS,pid,prior);
	syslog(LOG_ERR,"set priority (prior=%d)",prior);

	// init. rdb access
	if(rdb_init()<0) {
		syslog(LOG_ERR,"rdb_init() failed");
		goto err;
	}

	rdbfd=rdb_fd(rdb);


	// create varidx
	if(!(si_varidx=stridx_create())) {
		syslog(LOG_ERR,"failed to create si_varidx");
		goto err;
	}

	// create validx
	if(!(si_validx=stridx_create())) {
		syslog(LOG_ERR,"failed to create si_validx");
		goto err;
	}

	// build varidx and subscribe var
	p=rdbvar_to_varidx;
	while(p->str) {
		// add noti to si_varidx
		if(stridx_add(si_varidx,p->str,p->idx)<0) {
			syslog(LOG_ERR,"failed to register str(%s)",p->str);
			goto loop1_fini;
		}

		// create rdb variable
		if(rdb_create_string(rdb,p->str,"",0,0)<0) {
			if(errno!=EEXIST) {
				syslog(LOG_ERR,"failed to create rdb variable(%s) - %s",p->str,strerror(errno));
				goto loop1_fini;
			}
		}

		// subscribe
		if( rdb_subscribe(rdb,p->str)<0 ) {
			syslog(LOG_ERR,"failed to subscribe rdb variable(%s) - %s",p->str,strerror(errno));
			goto loop1_fini;
		}

	loop1_fini:
		p++;
	}

	// build validx
	p=rdbval_to_validx;
	while(p->str) {
		// add noti to si_validx
		if(stridx_add(si_validx,p->str,p->idx)<0) {
			syslog(LOG_ERR,"failed to register str(%s)",p->str);
			goto loop2_fini;
		}

	loop2_fini:
		p++;
	}

	// create rdb variables
	rdb_setVal("dispd.pattern.subtype","");


	return 0;
err:
	return -1;
}

void fini()
{
	if(si_varidx)
		stridx_destroy(si_varidx);
	if(si_validx)
		stridx_destroy(si_validx);

	rdb_fini();

	closelog();
}

int update_disps(const char* rdbvar,const char* val)
{
	long varidx;
	long validx;
	enum disp_idx_t dispidx;

	// parse rdbvar
	if(parse_rdb(rdbvar,val,&varidx,&validx,&dispidx)<0)
		goto err;

	// convert rdb to disp if out of range
	if(dispidx<0 || last_disp_ent<=dispidx) {
		if(convert_rdb_to_disp(rdbvar,val,varidx,validx,dispidx)<0)
			goto err;
	}
	else {
		if(copy_rdb_to_disp(rdbvar,val,varidx,validx,dispidx)<0)
			goto err;
	}

	return 0;

err:
	return -1;
}

int process_triggered_rdb()
{
	char names[RDB_TRIGGER_NAME_BUF];
	int names_len;
	const char* p;

	char val[RDB_VARIABLE_MAX_LEN];
	const char* rdbvar;

	char* token;
	long idx;

	char* saveptr;

	// get triggered rdbvar names
	names_len=sizeof(names);
	if( rdb_getnames(rdb,"",names,&names_len,TRIGGERED)<0 ) {
		//syslog(LOG_ERR,"rdb_get_names() failed - %s",strerror(errno));
		goto err;
	}
	names[names_len]=0;

	syslog(LOG_DEBUG,"[rdb] rdbvar notify list - %s",names);

	// extract tokens
	token=NULL;
	while( (token=strtok_r(!token?names:NULL,"&",&saveptr))!=NULL ) {

		rdbvar=token;

		// get idx
		if( (idx=stridx_find(si_varidx,rdbvar))<0 ) {
			syslog(LOG_ERR,"non-subscribed rdbvar variable triggered - %s",rdbvar);
			continue;
		}

		// read rdbvar
		if( !(p=rdb_getVal(rdbvar)) ) {
			syslog(LOG_ERR,"trigged but failed to read rdbvar(%s) - %s",rdbvar,strerror(errno));
			continue;
		}

		// store val
		strcpy(val,p);

		update_disps(rdbvar,val);
	}

	return 0;

err:
	return -1;
}

int process_all_rdbs()
{
	int idx;
	const char* var;
	const char* p;
	char val[RDB_VARIABLE_MAX_LEN];

	idx=stridx_get_first(si_varidx,&var);
	while(idx>=0) {
		// read rdb
		if( !(p=rdb_getVal(var)) ) {
			syslog(LOG_ERR,"trigged but failed to read rdb(%s) - %s",var,strerror(errno));
			continue;
		}

		// store val
		strcpy(val,p);

		update_disps(var,val);

		idx=stridx_get_next(si_varidx,&var);
	}

	return 0;
}

int print_usage()
{
	fprintf(stderr,
		"Usage: dispd [-v] [-V]\n"

		"\n"
		"Options:\n"

		"\t-v verbose mode (debugging log output)\n"
		"\t-V Display version information\n"
		"\n"

		"rdbs:\n"
		"\t[dispd.pattern.type]		LED pattern (resetbutton|mainflashing|recoveryflashing|factoryflashing)\n"
		"\n"

		"rdb examples:\n"
		"\trdb_set dispd.pattern.type resetbutton	# start reset button procedure\n"
		"\trdb_get dispd.pattern.subtype		# read reset LED status\n"
		"\n"
	);

	return 0;
}

int process_led_pattern()
{
	// maintain bootup period
	if( DISP_ENT_INT(booting_delay) && (now-poweron_time>=BOOTUP_DELAY)) {
		set_disp_ent_int(booting_delay,0);
		syslog(LOG_INFO,"[disp] boot period time-out - sec=%d sec",BOOTUP_DELAY);
	}

	// maintain screen dim-timer
	if(dimscreen_timer && !DISP_ENT_INT(dim_screen) && (now-screenon_time>=dimscreen_timer)) {
		set_disp_ent_int(dim_screen,1);
		syslog(LOG_INFO,"[led-off] dim screen time-out - timer=%ld(sec)",dimscreen_timer);
	}

	return 0;
}

int main(int argc,char* argv[])
{
	fd_set rfds;

	struct timeval tv;
	int retval;
	int ret;

	struct tms tmsbuf;

	// Parse Options
	while ((ret = getopt(argc, argv, "vVh")) != EOF)
	{
		switch (ret)
		{
			case 'v':
				verbosity=1;
				break;

			case 'V':
				fprintf(stderr, "dispd: build date / %s %s\n",__TIME__,__DATE__);
				return 0;

			case 'h':
			case '?':
				print_usage(argv);
				return 2;
		}
	}


	// init. disp
	init();

	// set timers
	cur=times(&tmsbuf);
	now=cur/TCK_PER_SEC;

	// init booting delay disp
	poweron_time=now;
	set_disp_ent_int(booting_delay,1);

	// init dimscreen disp
	screenon_time=now;
	set_disp_ent_int(dim_screen,0);

	#if defined(V_LEDFUN_140wx) || defined(V_LEDFUN_140) || defined(V_LEDFUN_nwl22) || defined(V_LEDFUN_nwl22w) || defined(V_LEDFUN_145w) || defined(V_LEDFUN_ntc_220)
	/* clear all unknown status of LEDs */
	syslog(LOG_INFO,"reset all LEDs to solid off");
	render_clear_leds(leds);
	#endif

	// walk through all subscriptions
	process_all_rdbs();

	// make render invalidate and render
	render_init();
	render_disps();

	// select loop
	while(1) {
		cur=times(&tmsbuf);
		now=cur/TCK_PER_SEC;

		// do dispd own dispd
		process_led_pattern();

		// render disps to physical leds
		render_disps();

		FD_ZERO(&rfds);
		FD_SET(rdbfd, &rfds);

		// wait one sec
		tv.tv_sec = seltimeout/1000;
		tv.tv_usec = (seltimeout%1000)*1000;

		retval = select(rdbfd+1, &rfds, NULL, NULL, &tv);

		// bypass if timed out
		if(retval==0) {
			continue;
		}

		// bypass if interrupted or exit if error condition
		if (retval<0) {
			if(errno==EINTR) {
				syslog(LOG_ERR,"punk!");
				continue;
			}

			syslog(LOG_ERR,"select() failed - %s",strerror(errno));
			goto err;
		}

		// give the execution to rdbnoti if rdb triggered
		if(FD_ISSET(rdbfd,&rfds)) {
			process_triggered_rdb();
		}

	}


	// fini. disp
	fini();

	return 0;
err:
	return -1;
}

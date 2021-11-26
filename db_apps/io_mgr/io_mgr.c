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
#include "rdb.h"
#include "ardb.h"
#include "xgpio.h"
#include "iio.h"
#include "hfi.h"
#include "libgpio.h"
#include "iir_lpfilter.h"
#include "atcp.h"
#include "tick64.h"
#include "stridx.h"
#include "binqueue.h"
#include "dq.h"
#include "commonIncludes.h"

/*
 * Globals
 */
struct io_info_t * io_info;
int io_cnt;
const char *modcommsExcludeList;

#define IO_INFO_SYSFS(t,n)	t #n

#define GPIO_BANK_PIN(b,p)	(((b)*32)+(p))

/* rdb reference */
#define MAKE_ARRAY_REF(r,r2)	(((r2)<<16) | (r))
#define REF2(r)			((r>>16) & 0x00007fff)
#define REF(r)			((r) & 0x0000ffff)

/* main loop select timeout ms - maximum gpio reaction delay */
#ifdef V_MODCOMMS
#define IO_MGR_SELECT_TIMEOUT 2000
#else
#define IO_MGR_SELECT_TIMEOUT 500
#endif

/* Bit mask for digital I/O */
#define DIN_INVERT 1
#define DOUT_INVERT 2
#define DIN_DOUT_INVERT 3


#warning [TBD] calbrate the frequencies (physical and rdb frequencies)
/* 10 Hz */
#define IO_MGR_PHYS_SAM_FREQ	20
/* 5 Hz */
#define IO_MGR_RDB_SAM_FREQ_X10	50

#define IO_MGR_INPUT_BUF_LEN	(10*1024)

int phys_sam_freq=IO_MGR_PHYS_SAM_FREQ; /* current hardware sampling frequency */
static int rdb_sam_freq_x10=IO_MGR_RDB_SAM_FREQ_X10; /* current rdb sampling frequency */

#if defined(V_IOMGR_falcon) || defined(V_IOMGR_ioext0) || defined(V_IOMGR_ioext0_s11) || defined(V_IOMGR_ioext1)
const int pull_up_voltage_gpio=GPIO_UNUSED;
#elif defined(V_IOMGR_ioext4)
const int pull_up_voltage_gpio=GPIO_BANK_PIN(2,4);
#elif defined(V_IOMGR_nguni)
const int pull_up_voltage_gpio=GPIO_BANK_PIN(2,25);
#elif defined(V_IOMGR_clarke)
const int pull_up_voltage_gpio=GPIO_BANK_PIN(1,31);
#elif defined(V_IOMGR_kudu)
const int pull_up_voltage_gpio=GPIO_UNUSED;
#else
#error V_IOMGR not specified
#endif

#warning [TODO] the following correction values have to be deleted. software should not correct the hardware errors

/* io pin pre-configuration */
static const struct io_info_t static_io_info[]={
#if defined(V_IOMGR_falcon)

	#warning [TBD] add a proper formula to get temperature
#if 0
	/* NWL10, NWL11 */
	{
		io_name:"temp",
  		power_type:IO_MGR_POWERSOURCE_NONE,
  		chan_name:IO_INFO_SYSFS("voltage",0),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
  		scale:IO_INFO_SCALE(1,1),
		correction:1,
  		d_in_threshold:0,
    		hardware_gain:1,
      		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
  		gpio_input_pin:GPIO_UNUSED,
    		gpio_pullup_pin:GPIO_UNUSED,
	},
#endif
	{
		io_name:"vin",
  		power_type:IO_MGR_POWERSOURCE_DCJACK,
  		chan_name:IO_INFO_SYSFS("voltage",1),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
  		scale:IO_INFO_SCALE(300,30),
		correction:1.00334168755, /* DCjack+POE */
  		d_in_threshold:0,
    		hardware_gain:1,
      		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
  		gpio_input_pin:GPIO_UNUSED,
    		gpio_pullup_pin:GPIO_UNUSED,
	},
	{
		io_name:"3v8poe",
  		power_type:IO_MGR_POWERSOURCE_POE,
  		chan_name:IO_INFO_SYSFS("voltage",3),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
  		scale:IO_INFO_SCALE(30,10),
		correction:0.9875,
  		d_in_threshold:0,
    		hardware_gain:1,
      		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
  		gpio_input_pin:GPIO_UNUSED,
    		gpio_pullup_pin:GPIO_UNUSED,
	},
	{
		io_name:"3v3",
  		power_type:IO_MGR_POWERSOURCE_NONE,
  		chan_name:IO_INFO_SYSFS("voltage",4),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
  		scale:IO_INFO_SCALE(30,10),
		correction:0.993,
  		d_in_threshold:0,
    		hardware_gain:1,
      		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
  		gpio_input_pin:GPIO_UNUSED,
    		gpio_pullup_pin:GPIO_UNUSED,
	},
	{
		io_name:"3v8",
  		power_type:IO_MGR_POWERSOURCE_NONE,
  		chan_name:IO_INFO_SYSFS("voltage",2),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
  		scale:IO_INFO_SCALE(30,10),
		correction:0.9915,
  		d_in_threshold:0,
    		hardware_gain:1,
      		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
  		gpio_input_pin:GPIO_UNUSED,
    		gpio_pullup_pin:GPIO_UNUSED,
	},


#elif defined(V_IOMGR_ioext4)

#if 0
	/* NWL12 */
	{
		io_name:"temp",
  		power_type:IO_MGR_POWERSOURCE_NONE,
  		chan_name:IO_INFO_SYSFS("voltage",0),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
  		scale:IO_INFO_SCALE(1,1),
		correction:1,
  		d_in_threshold:0,
    		hardware_gain:1,
      		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
  		gpio_input_pin:GPIO_UNUSED,
    		gpio_pullup_pin:GPIO_UNUSED,
	},
#endif
	{
		io_name:"vin",
  		power_type:IO_MGR_POWERSOURCE_DCJACK,
  		chan_name:IO_INFO_SYSFS("voltage",1),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
  		scale:IO_INFO_SCALE(300,20),
		correction:1.01180438449, /* DCjack+POE */
  		d_in_threshold:0,
    		hardware_gain:1,
      		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
  		gpio_input_pin:GPIO_UNUSED,
    		gpio_pullup_pin:GPIO_UNUSED,
	},

#ifndef V_PRODUCT_ntc_nwl25
	{
		io_name:"3v8poe",
  		power_type:IO_MGR_POWERSOURCE_POE,
  		chan_name:IO_INFO_SYSFS("voltage",3),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
		scale:IO_INFO_SCALE(30,10),
		correction:0.9925,
		d_in_threshold:0,
		hardware_gain:1,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
  		gpio_input_pin:GPIO_UNUSED,
    		gpio_pullup_pin:GPIO_UNUSED,
	},
#endif
	
	{
		io_name:"ign",
  		power_type:IO_MGR_POWERSOURCE_NONE,
  		chan_name:NULL,
		cap:IO_INFO_CAP_MASK_DIN,
		scale:0,
		correction:0,
		d_in_threshold:0,
		hardware_gain:0,
		init_mode:IO_INFO_CAP_MASK_DIN,
		gpio_output_pin:GPIO_BANK_PIN(2,9),
		gpio_input_pin:GPIO_BANK_PIN(2,9),
		gpio_pullup_pin:GPIO_UNUSED,
		gpio_dio_invert:DIN_DOUT_INVERT,
	},

	{
		io_name:"xaux1",
		power_type:IO_MGR_POWERSOURCE_NONE,
  		chan_name:IO_INFO_SYSFS("voltage",2),
#ifdef V_PRODUCT_vdf_nwl12
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN|IO_INFO_CAP_MASK_DOUT|IO_INFO_CAP_MASK_NAMUR|IO_INFO_CAP_MASK_CC,
#else
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN|IO_INFO_CAP_MASK_DOUT,
#endif
		scale:IO_INFO_SCALE(300,36.5)*2,
		correction:1,
		d_in_threshold:0,
		hardware_gain:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_BANK_PIN(2,20),
		gpio_input_pin:GPIO_BANK_PIN(2,20),
		gpio_pullup_pin:GPIO_BANK_PIN(2,5),
	},

	{
		io_name:"xaux2",
  		power_type:IO_MGR_POWERSOURCE_NONE,
  		chan_name:IO_INFO_SYSFS("voltage",4),
#ifdef V_PRODUCT_vdf_nwl12
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN|IO_INFO_CAP_MASK_DOUT|IO_INFO_CAP_MASK_NAMUR|IO_INFO_CAP_MASK_CC,
#else
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN|IO_INFO_CAP_MASK_DOUT,
#endif
		scale:IO_INFO_SCALE(300,36.5)*2,
		correction:1,
		d_in_threshold:0,
		hardware_gain:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_BANK_PIN(3,25),
		gpio_input_pin:GPIO_BANK_PIN(3,25),
		gpio_pullup_pin:GPIO_BANK_PIN(2,16),
	},

	{
		io_name:"xaux3",
  		power_type:IO_MGR_POWERSOURCE_NONE,
  		chan_name:IO_INFO_SYSFS("voltage",5),
#ifdef V_PRODUCT_vdf_nwl12
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN|IO_INFO_CAP_MASK_DOUT|IO_INFO_CAP_MASK_NAMUR|IO_INFO_CAP_MASK_CC,
#else
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN|IO_INFO_CAP_MASK_DOUT,
#endif
		scale:IO_INFO_SCALE(300,36.5)*2,
		correction:1,
		d_in_threshold:0,
		hardware_gain:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_BANK_PIN(3,27),
		gpio_input_pin:GPIO_BANK_PIN(2,27),
		gpio_pullup_pin:GPIO_BANK_PIN(2,17),
	},
#define LAST_XAUX_IDX "3"
#elif defined(V_IOMGR_ioext0)
	{
		io_name:"vin",
		power_type:IO_MGR_POWERSOURCE_DCJACK,
		chan_name:IO_INFO_SYSFS("voltage",1),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
		scale:IO_INFO_SCALE(300,20),
		correction:1.01180438449, /* DCjack+POE */
		d_in_threshold:0,
		hardware_gain:1,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
		gpio_input_pin:GPIO_UNUSED,
		gpio_pullup_pin:GPIO_UNUSED,
	},
#elif defined(V_IOMGR_ioext0_s11)
	{
		io_name:"vin",
		power_type:IO_MGR_POWERSOURCE_DCJACK,
		chan_name:IO_INFO_SYSFS("voltage",1),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
		scale:IO_INFO_SCALE(300, 30),
		correction:1,
		d_in_threshold:0,
		hardware_gain:1,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
		gpio_input_pin:GPIO_UNUSED,
		gpio_pullup_pin:GPIO_UNUSED,
	},
#elif defined(V_IOMGR_ioext1)
	{
		io_name:"vin",
		power_type:IO_MGR_POWERSOURCE_DCJACK,
		chan_name:IO_INFO_SYSFS("voltage",1),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
		scale:IO_INFO_SCALE(300, 30),
		correction:1,
		d_in_threshold:0,
		hardware_gain:1,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
		gpio_input_pin:GPIO_UNUSED,
		gpio_pullup_pin:GPIO_UNUSED,
	},

	{
		io_name:"ign",
		power_type:IO_MGR_POWERSOURCE_NONE,
		chan_name:NULL,
		cap:IO_INFO_CAP_MASK_DIN,
		scale:0,
		correction:0,
		d_in_threshold:0,
		hardware_gain:0,
		init_mode:IO_INFO_CAP_MASK_DIN,
		gpio_output_pin:GPIO_BANK_PIN(2,16),
		gpio_input_pin:GPIO_BANK_PIN(2,16),
		gpio_pullup_pin:GPIO_UNUSED,
		gpio_dio_invert:DIN_DOUT_INVERT,
	},
#elif defined(V_IOMGR_nguni)
	{
		io_name:"vin",
		power_type:IO_MGR_POWERSOURCE_DCJACK,
		chan_name:IO_INFO_SYSFS("voltage",6),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
  		scale:IO_INFO_SCALE(300,20),
		correction:1, /* DCjack+POE */
  		d_in_threshold:0,
    		hardware_gain:0,
      		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
  		gpio_input_pin:GPIO_UNUSED,
    		gpio_pullup_pin:GPIO_UNUSED,
	},

	{
		io_name:"ign",
  		power_type:IO_MGR_POWERSOURCE_NONE,
  		chan_name:IO_INFO_SYSFS("voltage",5),
		cap:IO_INFO_CAP_MASK_AIN,
		scale:0,
		correction:0,
		d_in_threshold:0,
		hardware_gain:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
		gpio_input_pin:GPIO_BANK_PIN(0,7),
		gpio_pullup_pin:GPIO_UNUSED,
		gpio_dio_invert:DIN_DOUT_INVERT,
	},

	{
		io_name:"8v2",
		power_type:IO_MGR_POWERSOURCE_NONE,
  		chan_name:IO_INFO_SYSFS("voltage",4),
		cap:IO_INFO_CAP_MASK_AIN,
		scale:0,
		correction:0,
		d_in_threshold:0,
		hardware_gain:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
  		gpio_input_pin:GPIO_UNUSED,
    		gpio_pullup_pin:GPIO_UNUSED,
	},

	{
		io_name:"xaux1",
  		power_type:IO_MGR_POWERSOURCE_NONE,
  		chan_name:IO_INFO_SYSFS("voltage",3),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN|IO_INFO_CAP_MASK_DOUT,
		scale:IO_INFO_SCALE(300,20),
		correction:1,
		d_in_threshold:0,
		hardware_gain:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_BANK_PIN(1,28),
		gpio_input_pin:GPIO_BANK_PIN(1,28),
		gpio_pullup_pin:GPIO_BANK_PIN(1,30),
	},
#define LAST_XAUX_IDX "1"
#elif defined(V_IOMGR_kudu)
	{
		io_name:"vin",
		power_type:IO_MGR_POWERSOURCE_DCJACK,
		chan_name:IO_INFO_SYSFS("voltage",6),
		cap:IO_INFO_CAP_MASK_AIN,
		scale:IO_INFO_SCALE(470,20),
		correction:1,
		d_in_threshold:0,
		hardware_gain:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
		gpio_input_pin:GPIO_UNUSED,
		gpio_pullup_pin:GPIO_UNUSED,
		gpio_1wire_pin:GPIO_UNUSED,
		gpio_oe:GPIO_UNUSED,
	},
	{
		io_name:"ign",
		power_type:IO_MGR_POWERSOURCE_NONE,
		chan_name:NULL,  //IO_INFO_SYSFS("voltage",5),
		cap:IO_INFO_CAP_MASK_AIN,
		scale:1,
		correction:1,
		d_in_threshold:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
		gpio_input_pin:GPIO_BANK_PIN(0,7),
		gpio_pullup_pin:GPIO_UNUSED,
		gpio_dio_invert:DIN_DOUT_INVERT,
		gpio_1wire_pin:GPIO_UNUSED,
		gpio_oe:GPIO_UNUSED,
	},
	{
		io_name:"xaux1",
		power_type:IO_MGR_POWERSOURCE_NONE,
		chan_name:NULL,
#ifdef V_ONE_WIRE
		cap:IO_INFO_CAP_MASK_DIN|IO_INFO_CAP_MASK_DOUT|IO_INFO_CAP_MASK_1WIRE,
#else
		cap:IO_INFO_CAP_MASK_DIN|IO_INFO_CAP_MASK_DOUT,
#endif
		scale:1,
		correction:1,
		d_in_threshold:0,
		init_mode:IO_INFO_CAP_MASK_DIN,
		gpio_output_pin:GPIO_BANK_PIN(2,25),
		gpio_input_pin:GPIO_BANK_PIN(2,25),
		gpio_pullup_pin:GPIO_UNUSED,
#ifdef V_ONE_WIRE
		gpio_1wire_pin:GPIO_BANK_PIN(2,25),
#else
		gpio_1wire_pin:GPIO_UNUSED,
#endif
		gpio_oe:GPIO_BANK_PIN(2,22),
	},
#define LAST_XAUX_IDX "1"
#elif defined(V_IOMGR_clarke)
	{
		io_name:"vin",
		power_type:IO_MGR_POWERSOURCE_DCJACK,
		chan_name:IO_INFO_SYSFS("voltage",6),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN,
		scale:IO_INFO_SCALE(470,20),
		correction:1, /* DCjack+POE */
		d_in_threshold:0,
		hardware_gain:1,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
		gpio_input_pin:GPIO_UNUSED,
		gpio_pullup_pin:GPIO_UNUSED,
	},
	{
		io_name:"ign",
		power_type:IO_MGR_POWERSOURCE_NONE,
		chan_name:NULL,
		cap:IO_INFO_CAP_MASK_AIN,
		scale:0,
		correction:0,
		d_in_threshold:0,
		hardware_gain:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_UNUSED,
		gpio_pullup_pin:GPIO_UNUSED,

	},
	{
		io_name:"xaux1",
		power_type:IO_MGR_POWERSOURCE_NONE,
		chan_name:IO_INFO_SYSFS("voltage",4),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN|IO_INFO_CAP_MASK_DOUT|IO_INFO_CAP_MASK_NAMUR|IO_INFO_CAP_MASK_CC,
		scale:IO_INFO_SCALE(300,36.5),
		correction:1,
		d_in_threshold:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_BANK_PIN(3,19),
		gpio_input_pin:GPIO_BANK_PIN(3,19),
		gpio_pullup_pin:GPIO_BANK_PIN(2,22),
		gpio_dio_invert:DOUT_INVERT,
	},
	{
		io_name:"xaux2",
		power_type:IO_MGR_POWERSOURCE_NONE,
		chan_name:IO_INFO_SYSFS("voltage",3),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN|IO_INFO_CAP_MASK_DOUT|IO_INFO_CAP_MASK_NAMUR|IO_INFO_CAP_MASK_CC,
		scale:IO_INFO_SCALE(300,36.5),
		correction:1,
		d_in_threshold:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_BANK_PIN(3,21),
		gpio_input_pin:GPIO_BANK_PIN(3,21),
		gpio_pullup_pin:GPIO_BANK_PIN(3,20),
		gpio_dio_invert:DOUT_INVERT,
	},
	{
		io_name:"xaux3",
		power_type:IO_MGR_POWERSOURCE_NONE,
		chan_name:IO_INFO_SYSFS("voltage",2),
		cap:IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN|IO_INFO_CAP_MASK_DOUT|IO_INFO_CAP_MASK_NAMUR|IO_INFO_CAP_MASK_CC,
		scale:IO_INFO_SCALE(300,36.5),
		correction:1,
		d_in_threshold:0,
		init_mode:IO_INFO_CAP_MASK_AIN,
		gpio_output_pin:GPIO_BANK_PIN(1,27),
		gpio_input_pin:GPIO_BANK_PIN(1,27),
		gpio_pullup_pin:GPIO_BANK_PIN(0,29),
		gpio_dio_invert:DOUT_INVERT,
	},
#define LAST_XAUX_IDX "3"
#else
#error V_IOMGR not specified
#endif

};

/* mode name table */
struct mode_name_tbl_t {
	const char* name;
	const int no;
};

static const struct mode_name_tbl_t mode_name_tbl[]={
	{"digital_input",IO_INFO_CAP_MASK_DIN},
	{"digital_output",IO_INFO_CAP_MASK_DOUT},
	{"analogue_input",IO_INFO_CAP_MASK_AIN},
	{"analogue_output",IO_INFO_CAP_MASK_AOUT},
	{"virtual_digital_input",IO_INFO_CAP_MASK_VDIN},
	{"1_wire",IO_INFO_CAP_MASK_1WIRE},
	{"current_input", IO_INFO_CAP_MASK_CIN},
	{"namurInput",IO_INFO_CAP_MASK_NAMUR},
	{"ctInput",IO_INFO_CAP_MASK_CT},
	{"contactClosureInput", IO_INFO_CAP_MASK_CC},
	{"tempInput",IO_INFO_CAP_MASK_TEMP},
};

/* io pin configuration */
struct io_cfg_t * io_cfg;

/* iir lowpassfilters */
struct iir_lpfilter_t * lpfilters;

struct power_source_t {
	int io_index;
	int powersource;
};

static struct power_source_t * power_source;
static int power_source_count=0;

/* numeric rdb variables */
enum {
	rdb_var_sampling_freq=0,
	rdb_var_rdb_sampling_freq,
	rdb_var_dbg_level,
 	rdb_var_daemon_enable,
	rdb_var_daemon_ready,
  	rdb_var_input_stream_mode,
  	rdb_var_daemon_watchdog,
	rdb_var_pull_up_voltage,

 	/* high frequency interface configuration */
	rdb_var_hfi_out_ipaddr,
 	rdb_var_hfi_out_port,
	rdb_var_hfi_in_ipaddr,
 	rdb_var_hfi_in_port,

 	rdb_var_power_source,

	rdb_var_last,
 	rdb_array_start=rdb_var_last,

	/* output - read */
	rdb_array_var_dac=rdb_array_start,
	rdb_array_var_d_out,
	rdb_array_var_pull_up_ctrl,

	/* input - write */
	rdb_array_var_adc,
	rdb_array_var_adc_raw,
 	rdb_array_var_adc_hw,
	rdb_array_var_d_in,
	rdb_array_var_d_invert,


	/* information - write */
 	rdb_array_var_cap,

  	/* configuration - subscribe(read) */
 	rdb_array_var_mode,
	rdb_array_var_d_in_threshold,

 	/* scale configuration - subscribe(read) */
 	rdb_array_var_scale,
  	rdb_array_var_correction,

   	/* individual hardware configuration */
   	rdb_array_var_hardware_gain,
#if defined(V_IOMGR_kudu)
	rdb_array_var_oe,
#endif
	rdb_array_var_last,
};

/* persistant table */
static const int rdb_persist_tbl[]={
	[rdb_var_sampling_freq]=0,
	[rdb_var_rdb_sampling_freq]=0,
 	[rdb_var_pull_up_voltage]=0,
	[rdb_var_dbg_level]=0,

 	/* high frequency interface */
	[rdb_var_hfi_out_ipaddr]=0,
	[rdb_var_hfi_out_port]=0,
	[rdb_var_hfi_in_ipaddr]=0,
	[rdb_var_hfi_in_port]=0,

	[rdb_var_power_source]=0,
 	[rdb_var_daemon_enable]=0,
	[rdb_var_input_stream_mode]=0,
	[rdb_var_daemon_watchdog]=0,

	[rdb_array_var_dac]=0,
	[rdb_array_var_d_out]=0,
	[rdb_array_var_pull_up_ctrl]=0,

	[rdb_array_var_adc]=0,
	[rdb_array_var_adc_raw]=0,
	[rdb_array_var_adc_hw]=0,

	[rdb_array_var_d_in]=0,
	[rdb_array_var_d_invert]=1,

	/* information - write */
 	[rdb_array_var_cap]=0,

  	/* configuration - subscribe(read) */
 	[rdb_array_var_mode]=1,
	[rdb_array_var_d_in_threshold]=1,

 	/* scale configuration - subscribe(read) */
 	[rdb_array_var_scale]=0,
  	[rdb_array_var_correction]=1,
   	[rdb_array_var_hardware_gain]=0,

#if defined(V_IOMGR_kudu)
	[rdb_array_var_oe]=1,
#endif
	[rdb_array_var_last]=0
};

/* string rdb variables */
const char* rdb_vars[]={

	[rdb_var_sampling_freq]="sys.sensors.iocfg.sampling_freq",
 	[rdb_var_rdb_sampling_freq]="sys.sensors.iocfg.rdb_sampling_freq",
  	[rdb_var_pull_up_voltage]="sys.sensors.iocfg.pull_up_voltage",

	[rdb_var_dbg_level]="sys.sensors.iocfg.mgr.debug",
	[rdb_var_daemon_enable]="sys.sensors.iocfg.mgr.enable",
	[rdb_var_daemon_ready]="sys.sensors.iocfg.mgr.ready",
	[rdb_var_input_stream_mode]="sys.sensors.iocfg.mgr.input_stream_mode",

	[rdb_var_daemon_watchdog]="sys.sensors.iocfg.mgr.watchdog",

 	[rdb_var_power_source]="sys.sensors.info.powersource",

	[rdb_array_var_dac]="sys.sensors.io.%s.dac",
	[rdb_array_var_d_out]="sys.sensors.io.%s.d_out",
 	[rdb_array_var_pull_up_ctrl]="sys.sensors.io.%s.pull_up_ctl",

	[rdb_array_var_adc]="sys.sensors.io.%s.adc",
	[rdb_array_var_adc_raw]="sys.sensors.io.%s.adc_raw",
	[rdb_array_var_adc_hw]="sys.sensors.io.%s.adc_hw",
	[rdb_array_var_d_in]="sys.sensors.io.%s.d_in",
	[rdb_array_var_d_invert]="sys.sensors.io.%s.d_invert",

 	/* high frequency interface configuration */
	[rdb_var_hfi_out_ipaddr]="sys.sensors.iocfg.hfi.out_ipaddr",
	[rdb_var_hfi_out_port]="sys.sensors.iocfg.hfi.out_port",
	[rdb_var_hfi_in_ipaddr]="sys.sensors.iocfg.hfi.in_ipaddr",
	[rdb_var_hfi_in_port]="sys.sensors.iocfg.hfi.in_port",

	/* information - write */
 	[rdb_array_var_cap]="sys.sensors.io.%s.cap",

  	/* configuration - subscribe(read) */
 	[rdb_array_var_mode]="sys.sensors.io.%s.mode",
	[rdb_array_var_d_in_threshold]="sys.sensors.io.%s.d_in_threshold",

 	/* scale configuration - subscribe(read) */
 	[rdb_array_var_scale]="sys.sensors.io.%s.scale",
  	[rdb_array_var_correction]="sys.sensors.io.%s.correction",
   	[rdb_array_var_hardware_gain]="sys.sensors.io.%s.hardware_gain",

#if defined(V_IOMGR_kudu)
	[rdb_array_var_oe]="sys.sensors.io.%s.oe",
#endif
	[rdb_array_var_last]=NULL
};

/* main loop running conditioin */
static int running=1;

/* per-channel down-sampling info */
struct ch_down_sampling_info_t {
	int valid;
	int count;
};
static struct ch_down_sampling_info_t * ch_down_sampling_info;


/* local functions */
void post_on_rdb_io_pin(int rdb,int pidx);
void on_rdb_io_pin(int rdb,int pidx, const char* val);
void enable_input_stream_mode(int en);


int getIo_Index(const char * io_name) {
	int i;
	/* check validation - check name */
	for(i=0;i<io_cnt;i++) {
		const struct io_info_t* io = &io_info[i];
		if (!io->chan_name)
			continue;
		if (0 == strcmp(io_name,io->chan_name) )
			return i;
	}
	return -1;
}


int sscanf_io_info_cap(const char* cap)
{
	char* cap2;
	char* sp;
	char* token;

	int i;

	int mode=0;

	/* duplicate string */
	cap2=strdup(cap);

	/* loop */
	token=strtok_r(cap2,"|",&sp);
	while(token) {
		/* search and merge a new mode */
		for(i=0;i<COUNTOF(mode_name_tbl);i++) {
			const struct mode_name_tbl_t * modep = &mode_name_tbl[i];
			if(!strcmp(token,modep->name))
				mode|=modep->no;
		}

		/* go next */
		token=strtok_r(NULL,"|",&sp);
	}

	free(cap2);

	return mode;
}

const char* snprintf_io_info_cap(int cap)
{
	static char v[RDB_VARIABLE_NAME_MAX_LEN];
    int i;
    char * p = 0;
    /*
     * scan table of modes and add the selected capabilities
     * - i.e. digital_output|analogue_input|virtual_digital_input
     */
    for(i=0;i<COUNTOF(mode_name_tbl);i++) {
        const struct mode_name_tbl_t * mode = &mode_name_tbl[i];
        if( cap & mode->no ) {
            if (p)
                *p++ = '|';
            else
                p = v;
            strcpy(p,mode->name);
            p += strlen(p);
        }
    }
	return v;
}

const char* snprintf_rdb_var(int numeric_rdb,int io_pin_idx)
{
	static char r[RDB_VARIABLE_NAME_MAX_LEN];
	const char* result;

	if(numeric_rdb<rdb_var_last) {
		result=rdb_vars[numeric_rdb];
	}
	else {
		/* check validation */
		if(io_pin_idx<0) {
			DBG(LOG_ERR,"invalid io pin idex detected (rdb=%d)",numeric_rdb);
			goto err;
		}

		snprintf(r,sizeof(r),rdb_vars[numeric_rdb],io_info[io_pin_idx].io_name);
		result=r;
	}

	/* check result validation */
	if(!*result) {
		DBG(LOG_ERR,"rdb string not found (rdb=%d,pidx=%d)",numeric_rdb,io_pin_idx);
		goto err;
	}

	return result;
err:
	return NULL;
}

void sig_handler(int signo)
{
	DBG(LOG_INFO,"punk! (signo=%d)",signo);

	if(signo==SIGTERM || signo==SIGINT)
		running=0;
}

#if defined(V_IOMGR_kudu)
/*
 * do some extra operations before changing I/O mode
 * @pidx: index of pin of which mode is being changed
 * @mode: new mode to which the pin is changed to
 */
static void prepare_change_io_mode (int pidx,int mode)
{
	const struct io_info_t* io;
	struct io_cfg_t* cfg;
	int mode_pattern = IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_DIN|IO_INFO_CAP_MASK_AOUT|IO_INFO_CAP_MASK_DOUT|IO_INFO_CAP_MASK_VDIN|IO_INFO_CAP_MASK_1WIRE;

	if (pidx >= io_cnt || !(mode&mode_pattern)) {
		return;
	}

	io = &io_info[pidx];
	cfg = &io_cfg[pidx];

	/*
	 * w1-gpio driver only probes with platform device and since then it requests GPIO until being removed.
	 * Hence if the pin is switched from 1-wire mode, the driver needs to be removed to release GPIO.
	 *
	 * If multiple digital-input/digital-ouput/1-wire pins are required, there should be enhancement:
	 * - user-space passes Start command with a GPIO to the driver; then the driver add and start a w1 master with that GPIO
	 * - user-space passes Stop command with a GPIO to the driver; then the driver remove the corresponding w1 master and release that GPIO
	 */

	/* currently it only needs to prepare from/to 1-wire mode, so return if gpio_1wire_pin is invalid */
	if (io->gpio_1wire_pin == GPIO_UNUSED) {
		return;
	}

	if (mode!=cfg->mode) {
		if (cfg->mode == IO_INFO_CAP_MASK_1WIRE) {
			/* switching from 1-wire mode */
			if (io->gpio_1wire_pin == io->gpio_output_pin || io->gpio_1wire_pin == io->gpio_input_pin) {
				system("rmmod w1-gpio");
				/* now request gpio */
				if (io->gpio_1wire_pin == io->gpio_output_pin) {
					gpio_request_pin(io->gpio_output_pin);
				}
				else {
					gpio_request_pin(io->gpio_input_pin);
				}
			}
		}
		else if (mode == IO_INFO_CAP_MASK_1WIRE) {
			/* switching to 1-wire mode from other modes */
			/* freeing gpio if necessary */
			if (io->gpio_1wire_pin == io->gpio_output_pin) {
				gpio_free_pin(io->gpio_output_pin);
			}
			else if (io->gpio_1wire_pin == io->gpio_input_pin) {
				gpio_free_pin(io->gpio_input_pin);
			}
			/* load 1_wire driver */
			system("modprobe w1-gpio");
		}

		/* dual supply translating transceiver OE is set by rdb_array_var_oe */
	}
}
#endif

int change_io_mode(int pidx,int mode)
{
	/*
	unfortunately, this change_io_mode() function may have some hardware dependency or may not.
	since, io mode switching can reqiure the understanding of controling other input / output pins.
	*/

	const struct io_info_t* io;
	struct io_cfg_t* cfg;

	/* get current io */
	io=&io_info[pidx];
	cfg=&io_cfg[pidx];

	if(!(io->cap & mode)) {
		DBG(LOG_ERR,"not supported mode (cap=0x%08x,req=0x%08x)",io->cap,mode);
		goto err;
	}

	// Intercept a modcomms device for special treatment
	if (io->dynamic && io->chan_name)
		return iio_set_mode(io,mode);

#if defined(V_IOMGR_kudu)
	prepare_change_io_mode(pidx, mode);
#endif

	switch(mode) {

		case IO_INFO_CAP_MASK_VDIN:
		case IO_INFO_CAP_MASK_AIN: {
			if(mode==IO_INFO_CAP_MASK_VDIN)
				DBG(LOG_DEBUG,"switching to virtual digital input (io=%s)",io->io_name);
			else
				DBG(LOG_DEBUG,"switching to analogue input (io=%s)",io->io_name);

			/* drive the gpio_output_pin low to turn off transistor*/
			if(io->gpio_output_pin!=GPIO_UNUSED) {
				DBG(LOG_INFO,"open gpio output (io=%s)",io->io_name);
				gpio_set_output(io->gpio_output_pin,0);
			}

			break;
		}

		case IO_INFO_CAP_MASK_DIN: {
			DBG(LOG_DEBUG,"switching to digital input (io=%s)",io->io_name);

			/* check validation */
			if(io->gpio_input_pin==GPIO_UNUSED) {
				DBG(LOG_ERR,"digital input pin not specified (io=%s)",io->io_name);
				goto err;
			}

			/* set input mode */
			if(gpio_set_input(io->gpio_input_pin)<0) {
				DBG(LOG_ERR,"failed to set gpio input mode (io=%s) - %s",io->io_name,strerror(errno));
				goto err;
			}
			break;
		}

		case IO_INFO_CAP_MASK_AOUT: {
			DBG(LOG_DEBUG,"switching to analogue output (io=%s)",io->io_name);
			#warning [TBD] switch to output analogue (DAC mode)
			break;
		}

		case IO_INFO_CAP_MASK_DOUT: {
			DBG(LOG_DEBUG,"switching to digital output (io=%s)",io->io_name);

			/*
				to call rdb handler, we set mode here exceptionally in this function
				mode setting has to be done in on_rdb_io_pin() and currently this is redundant
			*/
			cfg->mode=mode;

			/* simulate rdb set */
			post_on_rdb_io_pin(rdb_array_var_d_out,pidx);
			break;

		}
#if defined(V_IOMGR_kudu)
		case IO_INFO_CAP_MASK_1WIRE:
			break;
#endif
		default:
			DBG(LOG_ERR,"unknown mode detected (mode=0x%08x)",mode);
			break;
	}

	return 0;
err:
	return -1;
}

void post_on_rdb_io_pin(int rdb,int pidx)
{
	const char* v;
	char* v2;
	const char* r;

	/* get rdb name */
	r=snprintf_rdb_var(rdb,pidx);
	v=rdb_getVal(r);

	v2=strdup(v?v:"");

	/* simulate rdb set */
	on_rdb_io_pin(rdb,pidx,v2);

	free(v2);
}

void on_rdb_io_pin(int rdb,int pidx, const char* val)
{
	char* valptr;

	const struct io_info_t* io;
	struct io_cfg_t* cfg;

	/* get io & cfg */
	io=&io_info[pidx];
	cfg=&io_cfg[pidx];
	DBG(LOG_DEBUG,"on_rdb_io_pin (rdb=%d,pidx=%d) %s %s",rdb,pidx, io->io_name,val);

	switch(rdb) {
#if defined(V_IOMGR_kudu)
		case rdb_array_var_oe: {
			/* check validation */
			if(!*val) {
				DBG(LOG_DEBUG,"oe not specified (io=%s)",io->io_name);
				break;
			}

			/* check oe pin */
			if(io->gpio_oe==GPIO_UNUSED) {
				DBG(LOG_ERR,"oe pin not specified (io=%s)",io->io_name);
				break;
			}

			/* assert */
			int v=atoi(val);
			DBG(LOG_INFO,"change oe (pin=%d,str='%s',v=%d)",io->gpio_oe,val,v);

			/* digital output */
			if(gpio_set_output(io->gpio_oe,v)<0) {
				DBG(LOG_ERR,"failed to set gpio_oe (io=%s) - %s",io->io_name,strerror(errno));
			}
			//gpio_write(io->gpio_oe,v);
			break;
		}
#endif
		case rdb_array_var_mode: {
			int mode;

			/* check if it is blank */
			if(!*val) {
				DBG(LOG_DEBUG,"blank mode detected");
				goto fini_mode;
			}

			/* convert str to mode */
			mode=sscanf_io_info_cap(val);
			if(!mode) {
				DBG(LOG_ERR,"invalid mode detected (mode=%s)",val);
				goto fini_mode;
			}

			/* change mode */
			DBG(LOG_DEBUG,"change to mode (mode=0x%08x)",mode);
			if(change_io_mode(pidx,mode)<0) {
				DBG(LOG_ERR,"failed to change mode (mode=0x%08x)",mode);
				goto fini_mode;
			}

			/* store mode */
			cfg->mode=mode;

			#warning [TBD] write current mode to status (shadow) rdbs

		fini_mode:
			break;
		}

		case rdb_array_var_d_in_threshold: {
			double d_in_threshold;

			/* get d_in_threshold */
			d_in_threshold=strtod(val,&valptr);
			if(valptr==val) {
				d_in_threshold=io->d_in_threshold;
				DBG(LOG_DEBUG,"invalid d_in_threshold detected - use default %f",d_in_threshold);
			}

			/* store d_in_threshold */
			cfg->d_in_threshold=d_in_threshold;

			#warning [TODO] immediately update virtual digital output status based on the new threshold instead of waiting for a new value
			break;
		}

		case rdb_array_var_d_invert: {
			int invert;

			invert=atoi(val);
			cfg->gpio_dio_invert=invert;

			break;
		}

		case rdb_array_var_scale: {
			double scale;

			/* get scale */
			scale=strtod(val,&valptr);
			if(valptr==val) {
				scale=io->scale;
				DBG(LOG_DEBUG,"invalid scale detected - use default %f",scale);
			}

			/* store scale */
			cfg->scale=scale;

			break;
		}

		case rdb_array_var_hardware_gain: {
			int hardware_gain;

			/* bypass if no ain */
			if(!(io->cap & IO_INFO_CAP_MASK_AIN)) {
				DBG(LOG_DEBUG,"bypass hardware gain control (io=%s) - no AIN cap device detected",io->io_name);
				break;
			}

			/* get correction */
			hardware_gain=strtod(val,&valptr);
			if(valptr==val) {
				hardware_gain=io->hardware_gain;
				DBG(LOG_DEBUG,"invalid hardware_gain detected - use default %d",hardware_gain);
			}

			/* store hardware_gain */
			cfg->hardware_gain=hardware_gain;
#ifndef V_PROCESSOR_am335
			/*
			 * There are no "hardwaregain" entries in TI ADC driver (kernel v3.2.0), so this should not be applied on TI AM335.
			 * Moreover, after suspend/resume buffer in set_iio_ch_config_integer, the daemon won't receive ADC data from driver.
			 * Hence exclude V_PROCESSOR_am335 from this code.
			 * Of course, this can be different if TI AM335 ADC driver is upgraded/changed.
			 */
			DBG(LOG_DEBUG,"switch hardware (%s) to %d",io->chan_name,hardware_gain);
			if( set_iio_ch_config_integer(pidx,"hardwaregain",hardware_gain)>=0) {
				/* switch hardware gain */
				DBG(LOG_DEBUG,"switch hardware gain of channel (%s) to %d",io->io_name,hardware_gain);
			}
#endif
			break;
		}

		case rdb_array_var_correction: {
			double correction;

			/* get correction */
			correction=strtod(val,&valptr);
			if(valptr==val) {
				correction=io->correction;
				DBG(LOG_DEBUG,"invalid correction detected - use default %f",correction);
			}

			/* store correction */
			cfg->correction=correction;

			break;
		}

		case rdb_array_var_dac: {
			if(!*val) {
				DBG(LOG_DEBUG,"analog output not specified (pidx=%s)",io->io_name);
				break;
			}

			/* bypass if not AOUT mode */

			if(cfg->mode!=IO_INFO_CAP_MASK_AOUT) {
				DBG(LOG_ERR,"output not applied - incorrect mode (io=%s,mode=0x%08x)",io->io_name,cfg->mode);
				return;
			}

			/* assert */
			int v=atoi(val);
			DBG(LOG_INFO,"output analog IO=%s, str='%s',v=%d)",io->io_name,val,v);
			if(iio_set_output(io,v) < 0) {
				DBG(LOG_ERR,"failed in iio_set_output(analog output)");
				return;
			}

			break;
		}
		case rdb_array_var_pull_up_ctrl: {

			/* check validation */
			if(!*val) {
				DBG(LOG_DEBUG,"pull up not specified (io=%s)",io->io_name);
				break;
			}

			// Intercept a modcomms device for special treatment
			if (io->dynamic) {
				xgpio_set_pullup(io,val);
				break;
			}

			/* check pull-up pin */
			if(io->gpio_pullup_pin==GPIO_UNUSED) {
				DBG(LOG_ERR,"pull-up pin not specified (io=%s)",io->io_name);
				break;
			}

			/* assert */
			int v=atoi(val);
			DBG(LOG_INFO,"change pull-up (pin=%d,str='%s',v=%d)",io->gpio_pullup_pin,val,v);

			/* digital output */
			if(gpio_set_output(io->gpio_pullup_pin,v)<0) {
				DBG(LOG_ERR,"failed in gio_set_output(pullup) - %s",strerror(errno));
				break;
			}

			break;
		}

		case rdb_array_var_d_out: {
			if(!*val) {
				DBG(LOG_DEBUG,"digital output not specified (pidx=%s)",io->io_name);
				break;
			}

			/* bypass if not DOUT mode */

			if(cfg->mode!=IO_INFO_CAP_MASK_DOUT) {
				DBG(LOG_ERR,"output not applied - incorrect mode (io=%s,mode=0x%08x)",io->io_name,cfg->mode);
				break;
			}

			/* assert */
			int v=atoi(val);
			if(cfg->gpio_dio_invert & DOUT_INVERT)
				v=!v;

			// Intercept a modcomms device for special treatment
			if (io->dynamic && io->chan_name) {
				iio_set_output(io,v);
				break;
			}

			/* check validation */
			if(io->gpio_output_pin==GPIO_UNUSED) {
				DBG(LOG_ERR,"output pin not specified (io=%s,mode=0x%08x)",io->io_name,cfg->mode);
				break;
			}

			DBG(LOG_INFO,"output digital (pin=%d,str='%s',v=%d)",io->gpio_output_pin,val,v);
			if(gpio_set_output(io->gpio_output_pin,v)<0) {
				DBG(LOG_ERR,"failed in gpio_set_output(digital output) - %s",strerror(errno));
				break;
			}
			break;
		}
		default:
			DBG(LOG_ERR,"unknown pin rdb detected (rdb=%d)",rdb);
			break;
	}
}

void on_rdb_global(int rdb,const char* val)
{
	int v;
	const char* r;

	switch(rdb) {
		case rdb_var_daemon_watchdog: {
			/* bypass if already touched */
			if(!strcmp(val,"1")) {
				break;
			}

			DBG(LOG_INFO,"reset watchdog rdb");

			/* clear touch */
			r=snprintf_rdb_var(rdb_var_daemon_watchdog,-1);
			rdb_setVal(r,"1",rdb_persist_tbl[rdb_var_daemon_watchdog]);

			break;
		}

		case rdb_var_daemon_enable: {
			int enabled;
			const char* r;

			if(!*val) {
				DBG(LOG_DEBUG,"no daemon enable RDB specified");
				break;
			}

			enabled=atoi(val);
			if(!enabled) {
				r=snprintf_rdb_var(rdb_var_daemon_enable,-1);
				DBG(LOG_INFO,"daemon disabled by rdb #1 (rdb=%s,val='%s',enabled='%d')", r, val, enabled);
				running=0;
			}

			break;
		}

		case rdb_var_hfi_in_ipaddr:
		case rdb_var_hfi_out_ipaddr: {
			struct sockserver_t* ss;
			struct in_addr sin_addr;

			/* get a correct socketserver */
			if(rdb==rdb_var_hfi_in_ipaddr) {
				ss=&sservers[sockserver_input];
			}
			else {
				ss=&sservers[sockserver_output];
			}


			/* bypass if no ip address is specified */
			if(!*val) {
				DBG(LOG_DEBUG,"ip address not specified (server=%s)",sockserver_str_name[ss->idx]);
				break;
			}

			/* convert str to in_addr */
			if(!inet_aton(val,&sin_addr)) {
				DBG(LOG_ERR,"incorrect ip address specified (server='%s',val='%s')",sockserver_str_name[ss->idx],val);
				break;
			}

			/* reinit. if required */
			ss->ipaddr_valid=1;
			if(ss->saddr.sin_addr.s_addr!=sin_addr.s_addr || !atcp_is_server_running(ss->t)) {
				ss->saddr.sin_addr.s_addr=sin_addr.s_addr;
				sockserver_reinit(ss);
			}

			break;
		}

		case rdb_var_hfi_in_port:
		case rdb_var_hfi_out_port: {
			struct sockserver_t* ss;
			int port;

			/* get a correct socketserver */
			if(rdb==rdb_var_hfi_in_port) {
				ss=&sservers[sockserver_input];
			}
			else {
				ss=&sservers[sockserver_output];
			}

			/* bypass if no port is specified */
			if(!*val) {
				DBG(LOG_DEBUG,"port address not specified (server=%s)",sockserver_str_name[ss->idx]);
				break;
			}

			port=atoi(val);

			/* reinit. if required */
			ss->port_valid=1;
			if(ss->saddr.sin_port!=port || !atcp_is_server_running(ss->t)) {
				ss->saddr.sin_port=port;
				sockserver_reinit(ss);
			}
			break;
		}

		case rdb_var_dbg_level: {
			int logmask;
			int loglevel;

			/* use default loglevel if no value exists */
			if(!*val) {
				#ifdef DEBUG
				loglevel=LOG_DEBUG;
				DBG(LOG_DEBUG,"no debug level specified - use default LOG_DEBUG(%d)",loglevel);
				#else
				loglevel=LOG_INFO;
				DBG(LOG_DEBUG,"no debug level specified - use default LOG_INFO(%d)",loglevel);
				#endif
			}
			else {
				loglevel=atoi(val);
				DBG(LOG_INFO,"change loglevel to %d",loglevel);
			}

			/* set log mask */
			logmask=LOG_UPTO(loglevel);
			setlogmask(logmask);
			break;
		}

		case rdb_var_pull_up_voltage: {
			double voltage;
			char* valptr;

			/* bypass if no value is inputed */
			if(!*val) {
				DBG(LOG_DEBUG,"invalid pull up voltage specified (v='%s')",val);
				break;
			}

			/* bypass if no pull up voltage control is used */
			if(pull_up_voltage_gpio<0) {
				DBG(LOG_DEBUG,"no pull up voltage gpio specified");
				break;
			}

			/* get pull-up voltage */
			voltage=strtod(val,&valptr);
			if(valptr==val) {
				DBG(LOG_ERR,"invalid pull up voltage detected #1 (v='%s')",val);
				break;
			}

			/* check validation */
			if(voltage!=3 && voltage!=3.3 && voltage!=8.2) {
				DBG(LOG_ERR,"invalid pull up voltage detected #2 (v='%s')",val);
				break;
			}

			DBG(LOG_INFO,"change pull-up-voltage to %0.2f",voltage);

			/* assert */
			if(voltage==3 || voltage==3.3)
                         v=0;
			else
                         v=1;
			DBG(LOG_INFO,"output digital (pin=%d,str='%s',v=%d)",pull_up_voltage_gpio,val,v);
			if(gpio_set_output(pull_up_voltage_gpio,v)<0) {
				DBG(LOG_ERR,"failed in gpio_set_output(digital output) - %s",strerror(errno));
				break;
			}

			break;
		}

		case rdb_var_rdb_sampling_freq: {
			int sam_freq;

			sam_freq=atoi(val);

			if(!sam_freq) {
				DBG(LOG_DEBUG,"invalid rdb sampling frequency specified (cur_freq_x10=%d)",rdb_sam_freq_x10);
				break;
			}

			/* change sampling frequency */
			rdb_sam_freq_x10=sam_freq;
			DBG(LOG_DEBUG,"change rdb sampling frequency to %d",rdb_sam_freq_x10);
			break;
		}

		case rdb_var_sampling_freq: {
			int sam_freq;

			sam_freq=atoi(val);

			if(!*val) {
				// nothing to do
			}
			else if(!sam_freq) {
				DBG(LOG_ERR,"invalid sampling frequency specified (cur_freq=%d) - use current freq",phys_sam_freq);
			}
			else {
				phys_sam_freq=sam_freq;
			}

			/* change sampling frequency */
			iio_set_config_integer("configs/sampling_freq",phys_sam_freq);

			break;
		}

		default:
			DBG(LOG_ERR,"unknown global rdb detected (rdb=%d)",rdb);
			break;
	}

}

void on_select_rdb()
{
	const char* var;
	const char* val;
	char val2[RDB_VARIABLE_NAME_MAX_LEN];
	int array_rdb;

	/* get triggered rdb */
	int ref=ardb_get_first_triggered();
	while(ref>=0) {

		/* get references */
		int r=REF(ref);
		int r2=REF2(ref);

		/* check to see if it is array rdb */
		array_rdb=!(r<rdb_var_last);

		/* read the triggered rdb variable */
		var=snprintf_rdb_var(r,r2);
		val=rdb_getVal(var);
		if(!val) {
			DBG(LOG_ERR,"failed to read rdb (rdb=%s)",var);
			val="";
		}

		/* store val */
		STRNCPY(val2,val);

		/* if io pin rdb */
		if(array_rdb) {
			DBG(LOG_DEBUG,"io pin rdb triggered (r=%d,r2=%d,rdb=%s,val=%s)",r,r2,var,val2);
			on_rdb_io_pin(r,r2,val2);
		}
		else {
			DBG(LOG_DEBUG,"global rdb triggered (r=%d,r2=%d,rdb=%s,val='%s')",r,r2,var,val2);
			on_rdb_global(r,val2);
		}

		/* get next triggered rdb */
		ref=ardb_get_next_triggered();
	}
}


static void on_each_sampling( int ch_index,int value)
{
	const char* rdb;

	double adc;
	double adc_raw;
	int adc_hw;

	char val[RDB_VARIABLE_NAME_MAX_LEN];

	char ps_str[RDB_VARIABLE_MAX_LEN];
	int ps;
	static int prev_ps=-1;
	static int ps_delay=0;

	const char* r;
	int i;
	int j;
	int d_in;

	/* get io & cfg */
	struct io_cfg_t *cfg = &io_cfg[ch_index];

	/* get adc_raw and adc */
	adc_hw=value;
	convert_adc_hw_to_value(ch_index,adc_hw,&adc_raw,&adc);

	/* digital input by virtual digital input */
	d_in=adc>=cfg->d_in_threshold;
	if(cfg->gpio_dio_invert & DIN_INVERT)
		d_in=!d_in;

	if(cfg->mode==IO_INFO_CAP_MASK_VDIN && cfg->d_in!=d_in) {

		/* update digital in */
		cfg->d_in=d_in;

		/* build rdb var and val */
		rdb=snprintf_rdb_var(rdb_array_var_d_in,ch_index);
		snprintf(val,sizeof(val),"%d",cfg->d_in?1:0);

		/* set rdb */
		rdb_setVal(rdb,val,rdb_persist_tbl[rdb_array_var_d_in]);
	}

	/* store adc_hw to rdb */
	if(cfg->adc_hw!=adc_hw) {

		/* update adc_hw */
		cfg->adc_hw=adc_hw;

		/* build rdb var and val */
		rdb=snprintf_rdb_var(rdb_array_var_adc_hw,ch_index);
		snprintf(val,sizeof(val),"%d",cfg->adc_hw);

		/* set rdb */
		rdb_setVal(rdb,val,rdb_persist_tbl[rdb_array_var_adc_hw]);
	}

	/* store adc_raw to rdb */
	if(cfg->adc_raw!=adc_raw) {

		/* update adc_raw */
		cfg->adc_raw=adc_raw;

		/* build rdb var and val */
		rdb=snprintf_rdb_var(rdb_array_var_adc_raw,ch_index);
		snprintf(val,sizeof(val),"%f",cfg->adc_raw);

		/* set rdb */
		rdb_setVal(rdb,val,rdb_persist_tbl[rdb_array_var_adc_raw]);
	}

	/* store adc to rdb */
	if(cfg->adc!=adc) {

		/* update adc */
		cfg->adc=adc;

		/* build rdb var and val */
		rdb=snprintf_rdb_var(rdb_array_var_adc,ch_index);
		snprintf(val,sizeof(val),"%0.2f",cfg->adc);

		/* set rdb */
		rdb_setVal(rdb,val,rdb_persist_tbl[rdb_array_var_adc]);
	}

	if (!power_source)
		return;
	/*  get current power source - backward compability from voltage manager */
	ps=0;
	for(i=0;i<power_source_count;i++) {
		j=power_source[i].io_index;

		/* if any of power sources is not ready */
		if(io_cfg[j].adc_hw==-1) {

			/* wait until all the power sources are ready */
			if(ps_delay<power_source_count) {
				ps=0;
				ps_delay++;
				break;
			}
		}

		/* add up working power source */
		if(io_cfg[j].adc_hw>100)
			ps|=power_source[i].powersource;
	}

	/* apply new power source */
	if(ps!=prev_ps) {
		ps_str[0]=0;

		/* add dcjack */
		if(ps&IO_MGR_POWERSOURCE_DCJACK) {
			strcpy(ps_str,"DCJack");
		}
		/* add poe */
		if(ps&IO_MGR_POWERSOURCE_POE) {
			if(*ps_str)
				strcat(ps_str,"+");
			strcat(ps_str,"PoE");
		}

		DBG(LOG_INFO,"power source changed (ps=0x%08x,str='%s')",ps,ps_str);

		/* set rdb */
		r=snprintf_rdb_var(rdb_var_power_source,-1);
		rdb_setVal(r,ps_str,rdb_persist_tbl[rdb_var_power_source]);
	}
	prev_ps=ps;

}

static int do_down_sampling(int ch_index,unsigned int* ch_samples,int sample_count)
{
	/*
	* Make sure this cannot be 0 as we are using it in modulo division in the loop.
	* It is ok to calculate before the loop because variables phys_sam_freq and rdb_sam_freq_x10
	* (read from RDB) cannot change until this function exits as everything is done in one process/control loop.
	*/
	int down_sampling_divider=MAX(1, (int)(phys_sam_freq*10/rdb_sam_freq_x10));

	/* get finfo */
	struct ch_down_sampling_info_t* finfo = &ch_down_sampling_info[ch_index];
	/* get low-pass filter */
	struct iir_lpfilter_t* lpf = &lpfilters[ch_index];

	/* reset if invalid */
	if(!finfo->valid)
		finfo->count=0;
	finfo->valid=1;

	/* do down-sampling */
	int i;
	for(i=0;i<sample_count;i++) {
		/* apply low-pass filter - frequencies from 0% to 0.25% of the sampling frequency are allowed */
		int v = iir_lpfilter_filter(lpf,ch_samples[i]);

		if(!finfo->count)
			on_each_sampling(ch_index,v);

		finfo->count=(finfo->count+1)%down_sampling_divider;
	}

	return 0;
}

void on_iio_dev(void * iio_dev,int ch_index,unsigned int* ch_samples,int sample_count,unsigned long long ms64)
{
	int input_stream_mode;
	struct sockserver_t* ss;

	/* get input stream mode status */
	ss=&sservers[sockserver_input];
	input_stream_mode=ss->input_stream_mode;

	/* dump to high frequency interface only for traffic from iio */
	if(iio_dev) {
		dump_to_high_freq_interface(ch_index,ch_samples,sample_count,ms64);
	}

	/* when input mode is enabled, redirect input traffic only. */
	if( (input_stream_mode && !iio_dev) || !input_stream_mode) {
		/* perform iir filter and down-sampling */
		do_down_sampling(ch_index,ch_samples,sample_count);


		#if 0
		{
			static long long int total_cnt=0;

			static unsigned long long tm_s=0;
			static int tm_s_valid=0;

			unsigned long long now;

			now=tick64_get_ms();

			/* get startup time */
			if(!tm_s_valid) {
				tm_s=now;
				tm_s_valid=1;
			}

			if(ch_index==0) {
				total_cnt+=sample_count;

				if(now-tm_s)
					printf("%llu freq %llu samples\n",total_cnt*1000/(now-tm_s),total_cnt);
			}
		}
		#endif
	}
}

int do_gpio_update()
{
	const struct io_info_t* io;
	struct io_cfg_t* cfg;

	int i;
	int d_in;

	const char* val;

	const char* r;

	for(i=0;i<io_cnt;i++) {
		/* get current io */
		io=&io_info[i];
		cfg=&io_cfg[i];

		/* bypass non-din pins */
		if(!(cfg->mode&IO_INFO_CAP_MASK_DIN)) {
//			DBG(LOG_DEBUG,"bypassing non input pin(io=%s), mode %s",io->io_name, snprintf_io_info_cap(cfg->mode));
			continue;
		}
		/* bypass no input pin */
		if(io->gpio_input_pin==GPIO_UNUSED) {
			DBG(LOG_ERR,"gpio input pin not specified (io=%s)",io->io_name);
			continue;
		}
		/* read gpio */
		d_in= gpio_read(io->gpio_input_pin);
//		DBG(LOG_DEBUG,"poll pin(io=%s,gpio-%d) is %d",io->io_name,io->gpio_input_pin,d_in);
		if(d_in<0) {
			DBG(LOG_ERR,"failed in gpio_read(io=%s) - %s",io->io_name,strerror(errno));
			continue;
		}

		if(cfg->gpio_dio_invert & DIN_INVERT)
			d_in=!d_in;

		/* apply rdb if changed */
		if(d_in!=cfg->d_in) {

			/* get rdb var and val */
			r=snprintf_rdb_var(rdb_array_var_d_in,i);
			val=d_in?"1":"0";

			/* rdb_set */
			DBG(LOG_INFO,"set_rdb(var='%s',val=%s,d_in=%d)",r,val,d_in);
			rdb_setVal(r,val,rdb_persist_tbl[rdb_array_var_d_in]);
		}

		cfg->d_in=d_in;
	}

	return 0;
}

int gpio_start()
{
	int i;

	int output_pin;
	int input_pin;
#if defined(V_IOMGR_kudu)
	int oe;
#endif

	/* init. gpio driver */
	if(gpio_init("/dev/gpio")<0) {
		DBG(LOG_ERR,"failed to open gpio driver");
		goto err;
	}

	/* reserve pull up voltage control */
	if(pull_up_voltage_gpio>=0) {
		if(gpio_request_pin(pull_up_voltage_gpio)<0) {
			DBG(LOG_ERR,"failed in gpio_request_pin(pull_up_voltage_gpio) - %s",strerror(errno));
		}
	}

	/* reserve all gpio */
	for(i=0;i<io_cnt;i++) {

		const struct io_info_t* info=&io_info[i];

		/* get output pin */
		output_pin=GPIO_UNUSED;
		if(info->cap&IO_INFO_CAP_MASK_DOUT)
			output_pin=info->gpio_output_pin;

		/* get input pin */
		input_pin=GPIO_UNUSED;
		if(info->cap&IO_INFO_CAP_MASK_DIN)
			input_pin=info->gpio_input_pin;

		/* reserve output */
		if(output_pin!=GPIO_UNUSED) {
			if(input_pin!=output_pin)
				DBG(LOG_INFO,"reserve gpio (pin=%d) - output only",output_pin);
			else
				DBG(LOG_INFO,"reserve gpio (pin=%d) - input & output",output_pin);
			if(gpio_request_pin(output_pin)<0) {
				DBG(LOG_ERR,"failed in gpio_request_pin(output_pin) - %s",strerror(errno));
			}

			/* setup mux */
			if(gpio_gpio(output_pin)<0) {
				DBG(LOG_ERR,"failed in gpio_gpio(output_pin=%d) - %s",output_pin,strerror(errno));
				break;
			}

		}

		/* reserve input */
		if(input_pin!=GPIO_UNUSED && input_pin!=output_pin) {
			DBG(LOG_INFO,"reserve gpio (pin=%d) - input only",input_pin);
			if(gpio_request_pin(input_pin)<0) {
				DBG(LOG_ERR,"failed in gpio_request_pin(input_pin) - %s",strerror(errno));
			}

			/* setup mux */
			if(gpio_gpio(input_pin)<0) {
				DBG(LOG_ERR,"failed in gpio_gpio(input_pin=%d) - %s",input_pin,strerror(errno));
				break;
			}
		}
#if defined(V_IOMGR_kudu)
		/* reserve dual supply translating transceiver OE */
		oe=info->gpio_oe;
		if(oe!=GPIO_UNUSED) {
			DBG(LOG_INFO,"reserve gpio (pin=%d)",oe);
			if(gpio_request_pin(oe)<0) {
				DBG(LOG_ERR,"failed in gpio_request_pin(oe) - %s",strerror(errno));
			}
			/* setup mux */
			if(gpio_gpio(oe)<0) {
				DBG(LOG_ERR,"failed in gpio_gpio(oe=%d) - %s",oe,strerror(errno));
				break;
			}
		}
		/* other platform should reserve 1-wire for now if it != input and output */
#endif
		/* reserve pull up ctrl */
		if(info->gpio_pullup_pin!=GPIO_UNUSED) {
			DBG(LOG_INFO,"reserve gpio (pin=%d) - pull-up-ctrl",info->gpio_pullup_pin);
			if(gpio_request_pin(info->gpio_pullup_pin)<0) {
				DBG(LOG_ERR,"failed in gpio_request_pin(pullup-pin) - %s",strerror(errno));
			}

			/* setup mux */
			if(gpio_gpio(info->gpio_pullup_pin)<0) {
				DBG(LOG_ERR,"failed in gpio_gpio(pullup-pin=%d) - %s",info->gpio_pullup_pin,strerror(errno));
				break;
			}

		}
	}

	return 0;
err:
	return -1;
}

void gpio_stop()
{
	int output_pin;
	int input_pin;
	int i;

	/* free all gpio */
	for(i=0;i<io_cnt;i++) {
		/* get output pin */
		output_pin=GPIO_UNUSED;
		const struct io_info_t* info=&io_info[i];
#if defined(V_IOMGR_kudu)
		struct io_cfg_t *cfg = &io_cfg[i];
#endif
		if(info->cap&IO_INFO_CAP_MASK_DOUT)
			output_pin=info->gpio_output_pin;

		/* get input pin */
		input_pin=GPIO_UNUSED;
		if(info->cap&IO_INFO_CAP_MASK_DIN)
			input_pin=info->gpio_input_pin;

		/* release output */
		if(output_pin!=GPIO_UNUSED
#if defined(V_IOMGR_kudu)
				&& (cfg->mode != IO_INFO_CAP_MASK_1WIRE || output_pin!=info->gpio_1wire_pin)
#endif
		) {
			if(input_pin!=output_pin)
				DBG(LOG_INFO,"release gpio (pin=%d) - output only",output_pin);
			else
				DBG(LOG_INFO,"release gpio (pin=%d) - input & output",output_pin);

			gpio_free_pin(output_pin);
		}

		/* release input */
		if(input_pin!=GPIO_UNUSED && input_pin!=output_pin
#if defined(V_IOMGR_kudu)
				&& (cfg->mode != IO_INFO_CAP_MASK_1WIRE || input_pin!=info->gpio_1wire_pin)
#endif
		) {
			DBG(LOG_INFO,"release gpio (pin=%d) - input only",input_pin);
			gpio_free_pin(input_pin);
		}

		/* reserve pull up ctrl */
		if(info->gpio_pullup_pin!=GPIO_UNUSED) {
			DBG(LOG_INFO,"reserve gpio (pin=%d) - pull-up-ctrl",info->gpio_pullup_pin);
			gpio_free_pin(info->gpio_pullup_pin);
		}

#if defined(V_IOMGR_kudu)
		/* As w1-gpio driver is modprobe-ed by this daemon, it should be rmmod-ed at the end. */
		if (cfg->mode == IO_INFO_CAP_MASK_1WIRE && info->gpio_1wire_pin!=GPIO_UNUSED) {
			system("rmmod w1-gpio");
		}
		/* release Dual supply translating OE */
		if (info->gpio_oe!=GPIO_UNUSED) {
			gpio_free_pin(info->gpio_oe);
		}
#endif
	}

	/* free pull up voltage control */
	if(pull_up_voltage_gpio>=0) {
		gpio_free_pin(pull_up_voltage_gpio);
	}

	gpio_exit();
}



void print_usage(FILE* fd)
{
	fprintf(fd,
		"IO manager\n"

		"description:\n"
		"\tThis daemon controls external and internal I/O ports\n"
		"\n"

		"options:\n"
		"\t-i ignore IIO devices (run with no IIO device)\n"
		"\t-h print this usage screen\n"
				"\n"
	       );

}

void enable_input_stream_mode(int en)
{
	const char* r;
	/* turn off input stream mode */
	r=snprintf_rdb_var(rdb_var_input_stream_mode,-1);
	rdb_setVal(r,en?"1":"0",rdb_persist_tbl[rdb_var_input_stream_mode]);
}

int main(int argc,char* argv[])
{
	/* rdb to subscribe */
	static const int rdbs_to_subscribe[]={
		rdb_var_dbg_level,
		rdb_var_sampling_freq,
		rdb_var_rdb_sampling_freq,
		rdb_var_daemon_enable,
		rdb_var_pull_up_voltage,
		rdb_var_daemon_watchdog,

		/* high frequency interface configuration */
		rdb_var_hfi_out_ipaddr,
		rdb_var_hfi_out_port,
		rdb_var_hfi_in_ipaddr,
		rdb_var_hfi_in_port,
	};

	/* io rdbs to subscribe */
	static const int io_rdbs_to_subscribe[]={
		rdb_array_var_mode,
		rdb_array_var_d_in_threshold,
		rdb_array_var_scale,
		rdb_array_var_correction,
		rdb_array_var_hardware_gain,
		rdb_array_var_d_out,
		rdb_array_var_dac,
		rdb_array_var_pull_up_ctrl,
		rdb_array_var_d_invert,
#if defined(V_IOMGR_kudu)
		rdb_array_var_oe,
#endif
	};

	/* rdb to clear */
	static const int rdbs_to_clear[]={
		rdb_array_var_adc,
		rdb_array_var_adc_raw,
		rdb_array_var_adc_hw,
		rdb_array_var_d_in,
	};

	int i; /* general index */
	int j; /* general index2 */
	int idx; /* iio info index */
	const char* r; /* general char pointer */

	int hrdb; /* rdb file handle */

	const char* val; /* store rdb_getVal() result */
	char val2[RDB_VARIABLE_MAX_LEN]; /* general rdb content buffer */
	int rc; /* main result */
	int rdb_idx; /* numeric rdb - rdb index */
	int enabled; /* daemon enabled */

	rc=-1;

	int opt;

	int option_i=0;

	/* main select loop */
	fd_set readfds;
	struct timeval tv;
	int stat;

	/* get options */
	while ((opt = getopt(argc, argv, "ih")) != EOF) {
		switch(opt) {
			case 'h':
				print_usage(stdout);
				exit(0);
				break;

			case 'i':
				option_i=1;
				break;

			case ':':
				fprintf(stderr,"missing argument - %c\n",opt);
				print_usage(stderr);
				exit(-1);
				break;

			case '?':
				fprintf(stderr,"unknown option - %c\n",opt);
				print_usage(stderr);
				exit(-1);
				break;

			default:
				print_usage(stderr);
				exit(-1);
				break;
		}
	}

	/* open log */
	openlog("io_mgr",LOG_PID,LOG_DAEMON);

	/* set initial log mask to DEBUG */
	setlogmask(LOG_UPTO(LOG_DEBUG));

	/* init. tick */
	tick64_init();

	/* start rdb */
	ardb_init();

	/* copy the static definitions into into our dynamic array */
	io_info = (struct io_info_t *)malloc( sizeof(static_io_info) );
	if (!io_info)
		goto err;
	memcpy( io_info, static_io_info, sizeof(static_io_info) );
	io_cnt=COUNTOF(static_io_info);

#ifdef V_MODCOMMS
	val = rdb_getVal("modcomms.iomgr.exclude");
	if (val)
		modcommsExcludeList = strdup(val);
	xgpio_add_gpio_from_sysfs();
#else
#ifndef LAST_XAUX_IDX
#define LAST_XAUX_IDX "0"
#endif
	rdb_setVal("sys.sensors.info.lastio", LAST_XAUX_IDX, 0);
#endif
	if(!option_i) {
		/* create iio */
		if( iio_create() < 0 ) {
			DBG(LOG_ERR,"failed to create iio");
			goto err;
		}
	}

	/* init locals - filter */
	ch_down_sampling_info = (struct ch_down_sampling_info_t *)calloc(io_cnt, sizeof(struct ch_down_sampling_info_t) );
	if (!ch_down_sampling_info)
		goto err;

	/* init locals - cfg */
	io_cfg = (struct io_cfg_t*)calloc(io_cnt, sizeof(struct io_cfg_t) );
	if (!io_cfg)
		goto err;
	for(i=0;i<io_cnt;i++) {
		struct io_cfg_t* cfg=&io_cfg[i];
		cfg->adc_hw=-1;
		cfg->adc_raw=-1;
		cfg->adc=-1;
		cfg->d_in=-1;
	}

	/* create ch queues */
	if(ch_queue_init()<0) {
		DBG(LOG_ERR,"failed in ch_queue_init()");
		goto fini_ardb;
	}

	/* check daemon disable flag */
	r=snprintf_rdb_var(rdb_var_daemon_enable,-1);
	val=rdb_getVal(r);
	if(val && *val) {

		enabled=atoi(val);

		if(!enabled) {
			DBG(LOG_INFO,"daemon disabled by rdb #2 (rdb=%s,val='%s',enabled='%d')", r, val, enabled);
			rc=0;
			goto fini_ardb;
		}
	}

	/* start gpio */
	gpio_start();

	/* structure validation check */
	for(i=0;i<rdb_array_var_last;i++) {
		if(!rdb_vars[i]) {
			DBG(LOG_ERR,"internal structure error - rdb_var does not have a string (rdb=%d)",i);
			goto err;
		}
	}

	/* create tcp objects */
	memset(&sservers,0,sizeof(sservers));
	for(i=0;i<COUNTOF(sservers);i++) {
		/* create a server */
		struct sockserver_t* ss;
		struct atcp_t* t=atcp_create();
		if(!t) {
			DBG(LOG_ERR,"failed to create a tcp server (idx=%d)",i);
			goto err;
		}

		ss=&sservers[i];
		/* init. server members */
		ss->t = t;
		ss->idx=i;
		ss->port_valid=0;
		ss->ipaddr_valid=0;

		if(i==sockserver_input) {
			ss->in_sq=binqueue_create(IO_MGR_INPUT_BUF_LEN);
		}

		/* register callbacks and reference */
		atcp_set_callbacks(t,sockserver_on_accept,sockserver_on_read,ss);
	}

	/* create lowpass filters */
	lpfilters = (struct iir_lpfilter_t *)calloc(io_cnt, sizeof(struct iir_lpfilter_t) );
	if ( !lpfilters)
		goto err;
	// build power sources array
	// Allocate the maximum initially. It will be pruned later
	power_source = (struct power_source_t*)malloc(io_cnt*sizeof(struct power_source_t));
	if ( power_source ) {
		// Scan the IOs to see which are capable of supplying power
		j=0;
		for(i=0;i<io_cnt;i++) {
			const struct io_info_t* io = &io_info[i];
			if(io->power_type!=IO_MGR_POWERSOURCE_NONE) {
				power_source[j].io_index=i;
				power_source[j].powersource=io->power_type;
				j++;
				DBG(LOG_INFO,"vin power source found (io=%s,idx=%d,pt=%d)",io->io_name,i,io->power_type);
			}
		}
		power_source_count=j;
		power_source = (struct power_source_t*)realloc(power_source, power_source_count*sizeof(struct power_source_t));

		DBG(LOG_INFO,"%d power source(s) found ",power_source_count);
	}
	else {
		DBG(LOG_ERR,"Unable to allocate memory for power sources");
	}

	/* write global initial values */
	#define RDB_SET_IF_NOT_EXISTING(ridx,idx,defval,fmt) \
	do { \
		const char* __r; \
		const char* __val; \
		char __val2[RDB_VARIABLE_MAX_LEN]; \
		\
		__r=snprintf_rdb_var(ridx,idx); \
		__val=rdb_getVal(__r); \
		if(!__val || !*__val) { \
			snprintf(__val2,sizeof(__val2),fmt,defval); \
			DBG(LOG_DEBUG,"set rdb (rdb='%s',val='%s')",__r,__val2); \
			rdb_setVal(__r,__val2,rdb_persist_tbl[ridx]); \
		} \
	} while(0)

	#if 0
	/* rdb_var_sampling_freq */
	RDB_SET_IF_NOT_EXISTING(rdb_var_sampling_freq,-1,IO_MGR_PHYS_SAM_FREQ,"%d");
	/* rdb_var_dbg_level */
	RDB_SET_IF_NOT_EXISTING(rdb_var_dbg_level,-1,LOG_ERR,"%d");
	#endif

	/* turn off input stream mode */
	enable_input_stream_mode(0);

	RDB_SET_IF_NOT_EXISTING(rdb_var_hfi_out_ipaddr,-1,"127.0.0.1","%s");
	RDB_SET_IF_NOT_EXISTING(rdb_var_hfi_out_port,-1,30000,"%d");
	RDB_SET_IF_NOT_EXISTING(rdb_var_hfi_in_ipaddr,-1,"127.0.0.1","%s");
	RDB_SET_IF_NOT_EXISTING(rdb_var_hfi_in_port,-1,0,"%d");

	/* write global initial values - rdb_var_dbg_level */

	/* subscribe global variables */
	for(i=0;i<COUNTOF(rdbs_to_subscribe);i++) {
		rdb_idx=rdbs_to_subscribe[i];

		/* read initial rdb */
		r=snprintf_rdb_var(rdb_idx,-1);
		val=rdb_getVal(r);
		STRNCPY(val2,val?val:"");

		DBG(LOG_DEBUG,"read initial global rdb (r=%d,rdb=%s,val='%s')",rdb_idx,r,val2);
		on_rdb_global(rdb_idx,val2);
		/*
		 * on_rdb_global may invoke snprintf_rdb_var with different rdb index and make r stale.
		 * Hence recall snprintf_rdb_var here.
		 */
		r=snprintf_rdb_var(rdb_idx,-1);
		DBG(LOG_INFO,"subscribe global rdb (rdb=%s,ref=%d)",r,rdb_idx);
		if(ardb_subscribe(r,rdb_idx,rdb_persist_tbl[rdb_idx])<0) {
			DBG(LOG_ERR,"failed to subscribe (rdb=%s)",r);
		}
	}

	/* set io rdb variables - cap */
    for(idx=0;idx<io_cnt;idx++) {
        const struct io_info_t* io = &io_info[idx];
		/* create scale */
        RDB_SET_IF_NOT_EXISTING(rdb_array_var_scale,idx,io->scale,"%f");
		/* create correction */
        RDB_SET_IF_NOT_EXISTING(rdb_array_var_correction,idx,io->correction,"%f");
		/* create d_in_threshold */
        RDB_SET_IF_NOT_EXISTING(rdb_array_var_d_in_threshold,idx,io->d_in_threshold,"%f");
		/* create hardware_gain */
        RDB_SET_IF_NOT_EXISTING(rdb_array_var_hardware_gain,idx,io->hardware_gain,"%d");
		/* create mode */
        r=snprintf_io_info_cap(io->init_mode);
        RDB_SET_IF_NOT_EXISTING(rdb_array_var_mode,idx,r,"%s");
		/* create invert */
        RDB_SET_IF_NOT_EXISTING(rdb_array_var_d_invert,idx,io->gpio_dio_invert,"%d");

		/* set cap */
		r=snprintf_rdb_var(rdb_array_var_cap,idx);
        rdb_setVal(r,snprintf_io_info_cap(io->cap),rdb_persist_tbl[rdb_array_var_cap]);
	}

	/* subscribe io rdb variables */
	idx=0;
	for(idx=0;idx<io_cnt;idx++) {

		/* read rdb variables - subscribe */
		for(i=0;i<COUNTOF(io_rdbs_to_subscribe);i++) {
			rdb_idx=io_rdbs_to_subscribe[i];

			/* bypass for exceptions */
			if(rdb_idx==rdb_array_var_pull_up_ctrl && io_info[idx].gpio_pullup_pin==GPIO_UNUSED) {
				DBG(LOG_DEBUG,"do not create pull up ctrl (io=%s)",io_info[idx].io_name);
				continue;
			}

			/* read initial rdb */
			r=snprintf_rdb_var(rdb_idx,idx);
			val=rdb_getVal(r);
			STRNCPY(val2,val?val:"");

#if defined(V_IOMGR_clarke)
            if (rdb_idx==rdb_array_var_d_out) {
                STRNCPY(val2,atoi(val2)?"0":"1");
            }
#endif
			/* initially call */
			DBG(LOG_DEBUG,"read initial io pin rdb (r=%d,r2=%d,rdb=%s,val='%s')",rdb_idx,idx,r,val2);
			on_rdb_io_pin(rdb_idx,idx,val2);

			/* subscribe */
			/*
			 * on_rdb_io_pin may invoke snprintf_rdb_var with different rdb index and make r stale.
			 * Hence recall snprintf_rdb_var here.
			 */
			r=snprintf_rdb_var(rdb_idx,idx);
			DBG(LOG_INFO,"subscribe rdb (rdb=%s,ref=%d,ref2=%d)",r,rdb_idx,idx);
			if(ardb_subscribe(r,MAKE_ARRAY_REF(rdb_idx,idx),rdb_persist_tbl[rdb_idx])<0) {
				DBG(LOG_ERR,"failed to subscribe (rdb=%s)",r);
			}
		}

		/* write rdb variables - reset */
		for(i=0;i<COUNTOF(rdbs_to_clear);i++) {
			rdb_idx=rdbs_to_clear[i];

			r=snprintf_rdb_var(rdb_idx,idx);

			DBG(LOG_DEBUG,"reset rdb (r=%d,r2=%d,rdb=%s,val='%s')",rdb_idx,idx,r,"");
			rdb_setVal(r,"",rdb_persist_tbl[rdb_idx]);
		}
	}

	/* override signals */
	signal(SIGINT,sig_handler);
	signal(SIGTERM,sig_handler);
	signal(SIGPIPE,sig_handler);
	signal(SIGUSR2,sig_handler);

	/* turn on ready flag */
	r=snprintf_rdb_var(rdb_var_daemon_ready,-1);
	rdb_setVal(r,"1",rdb_persist_tbl[rdb_var_daemon_ready]);

	/* get handles */
	hrdb=rdb_get_handle();

	int timer_fd = timerfd_init(IO_MGR_SELECT_TIMEOUT);
	/* select loop */
	while(running) {
		//DBG(LOG_ERR,"select loop running");
		//DBG(LOG_ERR,"hrdb is %d, hiiodev is %d", hrdb, hiiodev);

		/* put input session stream queue to per-channel queue */
		dump_session_queue_to_ch_queue();
		/* feed ch queue to iio layer - emulate iio input */
		feed_ch_queue();

		/* init. readfds */
		FD_ZERO(&readfds);
		/* add rdb to readfds */
		FD_SET(hrdb,&readfds);
		fdsset_iio(&readfds);
		/* read gpios */
		#warning [TBD] use an hardware interrupt based signal instead of polling
		/* as Freescale cannot use interrupt on both edges, we need a generic way of doing this */
		int max_fd = timer_fd;
		int tmp_fd;
		/* setup tv */
		tv.tv_sec=(IO_MGR_SELECT_TIMEOUT/1000);
		tv.tv_usec=(IO_MGR_SELECT_TIMEOUT%1000)*1000;

		if ( timer_fd < 0 )
			do_gpio_update();
		else {
			FD_SET(timer_fd,&readfds);
			// We can have a bigger timeout here because we know timer_fd will get us out
			tv.tv_sec=5;
			tv.tv_usec=0;
		}
		/* apply fds from sockservers */
		for(i=0;i<COUNTOF(sservers);i++) {
			struct sockserver_t* ss=&sservers[i];

			/* bypass if the server is not running */
			if(!atcp_is_server_running(ss->t))
				continue;

			/* setup readfds */
			tmp_fd=atcp_set_fds(ss->t,&readfds);
			/* get max fd */
			if(tmp_fd>max_fd)
				max_fd=tmp_fd;
		}

		/* select */
		tmp_fd=MAX(hrdb,get_iio_maxhandle() );
		if(tmp_fd>max_fd)
			max_fd=tmp_fd;

		stat=select(max_fd+1,&readfds,NULL,NULL,&tv);

		/* break the loop if any error occurs */
		if(stat<0) {
			if(errno==EINTR) {
				DBG(LOG_INFO,"select() broken by an interrupt");
				continue;
			}

			DBG(LOG_ERR,"failed in select() - %s",strerror(errno));
			break;
		}

		/* collect all dead connections */
		sockserver_collect_dead_connections();

		/* continue if timeout occurs */
		if(stat==0) {
//			DBG(LOG_DEBUG,"timeout");
			continue;
		}

		/* process periodic timer */
		if(FD_ISSET(timer_fd,&readfds)) {
			timerfd_readTimer(timer_fd);
			do_gpio_update();
		}

		/* process serversocket */
		for(i=0;i<COUNTOF(sservers);i++) {
			struct sockserver_t* ss=&sservers[i];

			/* bypass if the server is not running */
			if(!atcp_is_server_running(ss->t))
				continue;

			atcp_do_process(ss->t,&readfds);
		}

		/* process rdb */
		if(FD_ISSET(hrdb,&readfds)) {
			DBG(LOG_INFO,"calling on_select_rdb");
			on_select_rdb();
		}
		process_iio(&readfds);
	}

	rc=0;

err:
	/* turn off ready flag */
	r=snprintf_rdb_var(rdb_var_daemon_ready,-1);
	rdb_setVal(r,"0",rdb_persist_tbl[rdb_var_daemon_ready]);

	/* stop buffering, fini buffering and destroy iio */
	iio_destroy();

	/* destroy tcp objects */
	for(i=0;i<COUNTOF(sservers);i++) {
		struct sockserver_t* ss=&sservers[i];
		atcp_destroy(ss->t);
		binqueue_destroy(ss->in_sq);
	}

	gpio_stop();

fini_ardb:
	ch_queue_fini();

	/* fini rdb */
	ardb_fini();

	safeFree(power_source);
	safeFree(io_cfg);
	/* destroy lowpass filters */
	safeFree(lpfilters);
	safeFree(ch_down_sampling_info);
	safeFree(modcommsExcludeList);

	if (io_info) {
		int i;
		for(i=0;i<io_cnt;i++) {
			struct io_info_t* io = &io_info[i];
			if(io->dynamic) {
				safeFree(io->io_name);
				safeFree(io->chan_name);
			}
		}
		free(io_info);
	}
	DBG(LOG_INFO,"terminated");

	return rc;
}


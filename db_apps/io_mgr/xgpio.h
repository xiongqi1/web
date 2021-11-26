#ifndef __XGPIO_H__27112015
#define __XGPIO_H__27112015
/*
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file contains the bulk of the Modcomms enhancements
 * For initialization it scans both the GPIO and IIO in /sys
 * GPIO IO goes through the existing IO_MGR paths
 * IIO IO is done here through /sys
 */

#define GPIO_UNUSED		(-1)
#define IO_INFO_CAP_MASK_AIN	(1<<0)
#define IO_INFO_CAP_MASK_DIN	(1<<1)
#define IO_INFO_CAP_MASK_AOUT	(1<<2)
#define IO_INFO_CAP_MASK_DOUT	(1<<3)
#define IO_INFO_CAP_MASK_1WIRE	(1<<5)

#define IO_INFO_CAP_MASK_CIN	(1<<6)
#define IO_INFO_CAP_MASK_NAMUR	(1<<7)
#define IO_INFO_CAP_MASK_CT	(1<<8)
#define IO_INFO_CAP_MASK_CC	(1<<9)
#define IO_INFO_CAP_MASK_TEMP	(1<<10)

#define IO_INFO_CAP_MASK_VDIN	(1<<16)

/*
 * Calculate the I/O scaling factor.
 *
 * From the schematic you will see a voltage divider:
 *   Vadc = I . R_bottom
 *   Where I = Vin/(R_top + R_bottom)
 *   Thus Vin = Vadc . (R_top + R_bottom)/R_bottom
 *   Ie.  Vin = Vadc . scalingFactor
 * Supports io_info_t.scale.
 * @param top     Top resistance value in voltage divider.
 * @param bottom  Bottom resistance value in voltage divider.
 * @return scalingFactor
 */
#define IO_INFO_SCALE(R_top, R_bottom) \
    ((((double)(R_top))+(R_bottom))/(R_bottom))

/* power sources (index to info) */
#define IO_MGR_POWERSOURCE_NONE		0
#define IO_MGR_POWERSOURCE_DCJACK	(1<<0)
#define IO_MGR_POWERSOURCE_POE		(1<<1)

/* io pin information */
struct io_info_t {
	char *io_name;
	int power_type;
	const char *chan_name;	// This is the channel name in sys fs tree - like "voltage1"
	int cap;				// capability - input|output etc
	double scale;           // To scale voltage divider measured voltage, @sa IO_INFO_SCALE
	double correction;
	double d_in_threshold;
	int hardware_gain;
	int init_mode;
	int gpio_output_pin;
	int gpio_input_pin;
	int gpio_pullup_pin;
	int gpio_dio_invert;
#if defined(V_IOMGR_kudu)
	int gpio_1wire_pin;
	int gpio_oe;
#endif
	int dynamic;			// This flag is set for dynamic modcomms IO
};

/* runtime pin configuration */
struct io_cfg_t {
	int mode;

	/*
		* configurable rdb representing members
		these rdb variables are initiated by info only when they do not exist
	*/
	double scale;
	double correction;
	double d_in_threshold;
	int hardware_gain;

	/*
		* run-time rdb representing members
		initial values of these rdb variables are -1
	*/

	int adc_hw;
	double adc_raw;
	double adc;

	int d_in;

	struct dq_t* out_vq; /* output queue */
	unsigned long long out_tm; /* start of output queue time */

	struct dq_t* in_vq; /* input queue */
	unsigned long long in_tm; /* start of input queue time */

	int gpio_dio_invert;
};

/* io pin configuration */
extern struct io_cfg_t * io_cfg;
extern struct io_info_t * io_info;
extern int io_cnt;
extern int xaux_cnt;	// This gives the next xaux number
extern const char *modcommsExcludeList;

int add_io( struct io_info_t * pNewIo, const char * label );
const char* snprintf_io_info_cap(int cap);
int sscanf_io_info_cap(const char* cap);
int getIo_Index(const char *io_name);

int xgpio_scan_uint( const char * ichars);
int xgpio_set_mode(const struct io_info_t * io ,int val);
void xgpio_set_pullup(const struct io_info_t * io ,const char * val);
void xgpio_add_gpio_from_sysfs();
int is_modcomms_device(const char * devname );
char * trimEnd(char * str );
int set_iio_ch_config_integer(int idx, const char* name,int value);

/* platform specific parameters */
#define IO_MGR_ADC_MAX		(4095)
#ifdef V_PROCESSOR_am335
#define IO_MGR_ADC_PHYS_MAX	(1.8)
#else
#define IO_MGR_ADC_PHYS_MAX	(1.85)
#endif

static int inline convert_value_to_adc_hw(int ch_index,double adc)
{
	int adc_hw;

	adc_hw=(adc*IO_MGR_ADC_MAX)/(io_cfg[ch_index].scale*io_cfg[ch_index].correction*IO_MGR_ADC_PHYS_MAX);

	return adc_hw;

}

static void inline convert_adc_hw_to_value(int ch_index,int adc_hw,double* adc_raw,double* adc)
{
	/* get adc_raw and adc */
	*adc_raw=(double)adc_hw*IO_MGR_ADC_PHYS_MAX/IO_MGR_ADC_MAX;
	*adc=*adc_raw*io_cfg[ch_index].scale*io_cfg[ch_index].correction;
}

#endif

/*!
 * Copyright Notice:
 * Copyright (C) 2009 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(PLATFORM_PLATYPUS)
#include <nvram.h>
#endif
#include "slic/calibration.h"
#include "cdcs_syslog.h"
#include "./pots_rdb_operations.h"

#define DEBUG_CAL

slic_cal_data_type slic_cal_data;
slic_cal_data_item_type dummy_cal_reg[MAX_CAL_REG_NO] =
{
	/*  reg addr.		name			cal.data */
	{ 771,		"MADC_VTIPC_OS",		0x00000000 },
	{ 772,		"MADC_VRINGC_OS",		0x00000000 },
	{ 773,		"MADC_VBAT_OS",			0x00000000 },
	{ 774,		"MADC_VLONG_OS",		0x00000000 },
	{ 776,		"MADC_VDC_OS",			0x00000000 },
	{ 777,		"MADC_ILONG_OS",		0x00000000 },
	{ 788,		"ACADC_OFFSET",			0x00000000 },
	{ 789,		"ACDAC_OFFSET",			0x00000000 },
	{ 778,		"MADC_ISNS_STDBY_OS",	0x00000000 },
	{ 779,		"MADC_ISNS_OS",			0x00000000 },
	{ 1462,		"LKG_DNT_ACTIVE",		0x00000000 },
	{ 1460,		"LKG_UPT_ACTIVE",		0x00000000 },
	{ 1463,		"LKG_DNR_ACTIVE",		0x00000000 },
	{ 1461,		"LKG_UPR_ACTIVE",		0x00000000 },
	{ 1466,		"LKG_DNT_STBY",			0x00000000 },
	{ 1465,		"LKG_UPT_STBY",			0x00000000 },
	{ 1467,		"LKG_DNR_STBY",			0x00000000 },
	{ 1465,		"LKG_UPR_STBY",			0x00000000 },
	{ 780,		"MADC_ILOOP_OS",		0x00000000 },
	{ 781,		"MADC_ILOOP_SCALE",		0x00000000 },
	{ 646,		"DC_DAC_GAIN",			0x00000000 },
	{ 1476,		"CMDAC_FWD",			0x00000000 },
	{ 1477,		"CMDAC_RVS",			0x00000000 }
};

void print_cal_data(slic_cal_data_type *cal_data_p)
{
#ifdef DEBUG_CAL
	slic_cal_data_enum_type item;
	int i;
	SYSLOG_DEBUG("SLIC CAL DATA : calibrated = %d", cal_data_p->calibrated);
	SYSLOG_DEBUG("SLIC CAL DATA : ch    addr    name       val");
	for (i = 0; i < MAX_CHANNEL_NO; i++)
	{
		for (item = MADC_VTIPC_OS; item < END_OF_CAL_DATA; item++)
		{
			SYSLOG_DEBUG("SLIC CAL DATA : %d    %d    %s    %d", i, cal_data_p->cal_data[i].reg[item].reg_address,
			             cal_data_p->cal_data[i].reg[item].reg_name,
			             cal_data_p->cal_data[i].reg[item].reg_value);
		}
	}
#endif
}

char* itoa(unsigned int val, int base)
{
	static char buf[32] = {0};
	int i = 30;

	if (val == 0)
	{
		buf[0] = '0';
		return &buf[0];
	}
	for (; val && i ; --i, val /= base)
		buf[i] = "0123456789abcdef"[val % base];
	return &buf[i+1];
}

void init_slic_cal_data_variable(void)
{
	slic_cal_data_enum_type item;
	int i;
	char *s;
	slic_cal_data.calibrated = 0;
	for (i = 0; i < MAX_CHANNEL_NO; i++)
	{
		for (item = MADC_VTIPC_OS; item < END_OF_CAL_DATA; item++)
		{
			slic_cal_data.cal_data[i].reg[item].reg_address = dummy_cal_reg[item].reg_address;
			slic_cal_data.cal_data[i].reg[item].reg_value = dummy_cal_reg[item].reg_value;
			s = &slic_cal_data.cal_data[i].reg[item].reg_name[0];
			sprintf(s, "%s%c", dummy_cal_reg[item].reg_name, '0' + i);
		}
	}
}

static int read_slic_cal_data_from_memory(void)
{
#ifdef STORE_CAL_DATA_TO_NVRAM_STORAGE
	slic_cal_data_enum_type item;
	int i;
#if defined(PLATFORM_PLATYPUS)
	char *rd_data;
#else
	char rd_data[10];
#endif

#if defined(PLATFORM_PLATYPUS)
	nvram_init(RT2860_NVRAM);
#endif
	for (i = 0; i < MAX_CHANNEL_NO; i++)
	{
		for (item = CMDAC_FWD; item < END_OF_CAL_DATA; item++)
		{
#if defined(PLATFORM_PLATYPUS)
			rd_data = nvram_bufget(RT2860_NVRAM, slic_cal_data.cal_data[i].reg[item].reg_name);
			if (!rd_data || strcmp(rd_data, "") == 0)
				goto db_error;
#else
			(void) memset((void *)&rd_data[0], 0x00, 10);
			if (rdb_get_single(slic_cal_data.cal_data[i].reg[item].reg_name, rd_data, 10) != 0)
				goto db_error;
			if (strcmp(rd_data, "") == 0)
				goto db_error;
#endif
			slic_cal_data.cal_data[i].reg[item].reg_value = (unsigned int) atoi(rd_data);
			SYSLOG_DEBUG("read NV item : %s : %d : 0x%x", slic_cal_data.cal_data[i].reg[item].reg_name,
			             slic_cal_data.cal_data[i].reg[item].reg_value, slic_cal_data.cal_data[i].reg[item].reg_value);
#if defined(PLATFORM_PLATYPUS)
			nvram_strfree(rd_data);
#endif
		}
	}
	slic_cal_data.calibrated = 1;
#if defined(PLATFORM_PLATYPUS)
	nvram_close(RT2860_NVRAM);
#endif
	return 1;

db_error:

#if defined(PLATFORM_PLATYPUS)
	nvram_strfree(rd_data);
	nvram_close(RT2860_NVRAM);
#endif
	slic_cal_data.calibrated = 0;
	return 0;

#else
	// temporally return 1 always...
	slic_cal_data.calibrated = 1;
	return 1;
#endif
}

int write_slic_cal_data_to_NV(BOOL force_cal)
{
#ifdef STORE_CAL_DATA_TO_NVRAM_STORAGE
	slic_cal_data_enum_type item;
	int i;
	int result;
	char *s, *v;

#if defined(PLATFORM_PLATYPUS)
	nvram_init(RT2860_NVRAM);
#endif
	for (i = 0; i < MAX_CHANNEL_NO; i++)
	{
		for (item = CMDAC_FWD; item < END_OF_CAL_DATA; item++)
		{
			s = &slic_cal_data.cal_data[i].reg[item].reg_name[0];
			sprintf(s, "%s%c", dummy_cal_reg[item].reg_name, '0' + i);
			v = itoa(slic_cal_data.cal_data[i].reg[item].reg_value, 10);
#if defined(PLATFORM_PLATYPUS)
			result = nvram_bufset(RT2860_NVRAM, slic_cal_data.cal_data[i].reg[item].reg_name, v);
#else
			rdb_set_single(slic_cal_data.cal_data[i].reg[item].reg_name, v);
			result = 1;
#endif
			SYSLOG_DEBUG("write NV item : %s : %d : 0x%x", slic_cal_data.cal_data[i].reg[item].reg_name,
			             slic_cal_data.cal_data[i].reg[item].reg_value, slic_cal_data.cal_data[i].reg[item].reg_value);
			if (result < 0)
			{
				SYSLOG_ERR("write NV item failure: %s : %d : 0x%x", slic_cal_data.cal_data[i].reg[item].reg_name,
				           slic_cal_data.cal_data[i].reg[item].reg_value, slic_cal_data.cal_data[i].reg[item].reg_value);
				goto return_error;
			}
		}
	}
	/* do not save to NV in order to recalibrate next boot time */
#if defined(PLATFORM_PLATYPUS)
	result = (force_cal? 1:nvram_bufset(RT2860_NVRAM, "slic_calibrated", "1"));
#else
	result = 1;
	if (!force_cal)
		rdb_set_single("slic_calibrated", "1");
#endif
	if (result < 0)
	{
		SYSLOG_ERR("write NV item failure: %s : %s", "slic_calibrated", "1");
		goto return_error;
	}
#if defined(PLATFORM_PLATYPUS)
	result = nvram_commit(RT2860_NVRAM);
	if (result < 0)
	{
		SYSLOG_ERR("NV items commit failure");
		goto return_error;
	}
#endif

#if defined(PLATFORM_PLATYPUS)
	nvram_close(RT2860_NVRAM);
#endif
	slic_cal_data.calibrated = 1;
	return 1;

return_error:
#if defined(PLATFORM_PLATYPUS)
	nvram_close(RT2860_NVRAM);
#endif
	slic_cal_data.calibrated = 0;
	return 0;

#else
	// temporally return 1 always...
	slic_cal_data.calibrated = 1;
	return 1;
#endif
}

int validate_slic_cal_data(void)
{
#ifdef STORE_CAL_DATA_TO_NVRAM_STORAGE
	slic_cal_data_enum_type item;
	int i;
	int result;
	char *s;

	result = 1;
	for (i = 0; i < MAX_CHANNEL_NO; i++)
	{
		for (item = CMDAC_FWD; item < END_OF_CAL_DATA; item++)
		{
			if (slic_cal_data.cal_data[i].reg[item].reg_value == 0)
			{
				s = &slic_cal_data.cal_data[i].reg[item].reg_name[0];
				sprintf(s, "%s%c", dummy_cal_reg[item].reg_name, '0' + i);
				SYSLOG_DEBUG("CAL Failed : item %s = %d, retry", slic_cal_data.cal_data[i].reg[item].reg_name,
				             slic_cal_data.cal_data[i].reg[item].reg_value);
				result = 0;
				break;
			}
		}
	}
	return result;
#else
	// temporally return 1 always...
	return 1;
#endif
}

int is_slic_calibrated(BOOL force_cal)
{
#ifdef STORE_CAL_DATA_TO_NVRAM_STORAGE
#if defined(PLATFORM_PLATYPUS)
	char* calibrated;
#else
	char calibrated[10] = {0, };
#endif

	if (force_cal)
	{
		SYSLOG_DEBUG("calibrate by force...");
		return 0;
	}
#if defined(PLATFORM_PLATYPUS)
	nvram_init(RT2860_NVRAM);
	calibrated = nvram_get(RT2860_NVRAM, "slic_calibrated");
	SYSLOG_DEBUG("read NV item : slic_calibrated : %s", calibrated);
	if (!calibrated || strcmp(calibrated, "") == 0 || strcmp(calibrated, "0") == 0)
		goto db_error;
	nvram_strfree(calibrated);
	nvram_close(RT2860_NVRAM);
#else
	if (rdb_get_single("slic_calibrated", calibrated, 10) < 0)
		goto db_error;
	SYSLOG_DEBUG("read NV item : slic_calibrated : %s", calibrated);
	if (strcmp(calibrated, "") == 0 || strcmp(calibrated, "0") == 0)
		goto db_error;
#endif
	if (!read_slic_cal_data_from_memory())
	{
		SYSLOG_DEBUG("fail to read cal data");
		return 0;
	}
	return 1;

db_error:

#if defined(PLATFORM_PLATYPUS)
	nvram_strfree(calibrated);
	nvram_close(RT2860_NVRAM);
#endif
	SYSLOG_DEBUG("no cal data or cal data was erased!");
	return 0;

#else
	// temporally return 1 always...
	return 1;
#endif
}

/*
* vim:ts=4:sw=4:
*/

/*!
 *
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
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
 *  Miscellaneous support
 */

#include "utils.h"
#include "io_mgr.h"
#include <cdcs_shellfs.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

using namespace std;

#define IIO_MAX_SYSFS_CONTENT_LEN 64 /* maximum content length of each sysfs entry */
#define INTEGER_STRING_SIZE 32 // 32 characters should be more than enough to read an integer

// Read an integer from file path/filename
bool getIntFromFile(const string & path, const string & fileName, int *value)
{
	char str[INTEGER_STRING_SIZE];	// Enough storage for an integer string

	if( shellfs_cat_with_path( path.c_str(), fileName.c_str(), str, sizeof(str) ) <= 0 ) {
		return false;
	}
	/* reset errno */
	errno=0;
	/* convert */
	char* ep;
	*value=strtol(str,&ep,0);

	/* check error */
	if( errno || (*ep && !isspace(*ep)) ) {
		DBG(LOG_ERR,"failed to read integer - %s from file %s - %s", str, fileName.c_str(), strerror(errno) );
		return false;
	}
	return true;
}

// Trim trailing space and return pointer to end of string
char * trimEnd(char * str )
{
	int len = strlen(str);
	if (len < 1) {
		return str;
	}
	char * end = str + len - 1;
	while((end > str) && isspace(*end)) {
		end--;
	}
	// Write new null terminator
	*++end = 0;
	return end;
}

#ifdef V_MODCOMMS
int is_modcomms_device(const char * devname )
{
	return strstr(devname,"-mice") != 0;
}
#endif

/*
 * The following structure and routines are for converting the Io mode into their
 * descriptions and vice versa
 */
struct BitMaskDesc {
	const char* name;
	const int mode;
};

static const struct BitMaskDesc ioModeDescTbl[]={
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

/*
 * Input will be a string like "digital_input|digital_output"
 * and output will be IO_INFO_CAP_MASK_DIN|IO_INFO_CAP_MASK_DOUT
 */
int stringToCapabilities(const char* cap)
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
		for(i=0;i<COUNTOF(ioModeDescTbl);i++) {
			const struct BitMaskDesc * modep = &ioModeDescTbl[i];
			if(!strcmp(token,modep->name))
				mode |= modep->mode;
		}

		/* go next */
		token=strtok_r(NULL,"|",&sp);
	}

	free(cap2);
	return mode;
}

static string maskToString(const BitMaskDesc bitMaskDesc[], int cnt, uint mask)
{
	string result;
	bool needsSeperator = false;

	for (int i = 0; i < cnt; i++)
	{
		const struct BitMaskDesc * modep = &bitMaskDesc[i];
		if (mask & modep->mode)
		{
			if (needsSeperator)
				result += '|';
			result += modep->name;
			needsSeperator = true;
		}
	}
	return result;
}

/*
 * Input will be IO_INFO_CAP_MASK_DIN|IO_INFO_CAP_MASK_DOUT
 * and output will be a string like "digital_input|digital_output"
 */
string capabilitiesToString(int cap)
{
	/*
	 * scan table of modes and add the selected capabilities
	 * - i.e. digital_output|analogue_input|virtual_digital_input
	 */
	return maskToString(ioModeDescTbl, COUNTOF(ioModeDescTbl), cap);
}

#define IO_MGR_POWERSOURCE_DCJACK	(1<<13)
#define IO_MGR_POWERSOURCE_POE		(1<<14)
#define IO_MGR_POWERSOURCE_NMA1500	(1<<15)

static const struct BitMaskDesc powerDescTbl[]={
	{"DCJack",IO_MGR_POWERSOURCE_DCJACK},
	{"PoE",IO_MGR_POWERSOURCE_POE},
	{"NMA1500",IO_MGR_POWERSOURCE_NMA1500}
};

/*
 * Input will be IO_MGR_POWERSOURCE_DCJACK|IO_MGR_POWERSOURCE_POE
 * and output will be a string like "digital_input|digital_output"
 */
string powerToString(uint powerMask)
{
	/*
	 * scan table of modes and add the selected capabilities
	 * - i.e. digital_output|analogue_input|virtual_digital_input
	 */
	return maskToString(powerDescTbl, COUNTOF(powerDescTbl), powerMask);
}

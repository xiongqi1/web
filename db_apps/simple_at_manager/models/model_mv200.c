/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
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
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/times.h> 

#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "../util/rdb_util.h"

#include "../rdb_names.h"
#include "../at/at.h"
#include "../model/model.h"
#include "../util/scheduled.h"

#include "model_default.h"

static clock_t _clkInsert=0;
static clock_t _clkPerSec=0;

#define WAITTIME_SIMREADY		10	// seconds
#define WAITTIME_NWREG			20	// seconds
#define WAITTIME_ATT			30	// seconds

////////////////////////////////////////////////////////////////////////////////
static int mv200_init(void)
{
	struct tms tmsbuf;
	// get ticks per second
	_clkPerSec=sysconf(_SC_CLK_TCK); 
	// get start tick
	_clkInsert=times(&tmsbuf);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int mv200_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	/* TODO : many of the following functions should be moving to init or triggered by chg_status*/

//	mv200_update_sim_status();


	if(chg_status->status[model_status_sim_ready])
	{
		const char* szSIMStat;

		if(new_status->status[model_status_sim_ready])
			szSIMStat="SIM OK";
		else
			szSIMStat="SIM BUSY";

		rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), szSIMStat);
	}

	update_signal_strength();

	update_imsi();

	update_network_name();
	update_service_type();

	return 0;
}

char *strcasestr(const char *haystack, const char *needle);

////////////////////////////////////////////////////////////////////////////////
static int mv200_detect(const char* manufacture, const char* model_name)
{
	return strcasestr(manufacture,"AXESSTEL")!=0;
}

///////////////////////////////////////////////////////////////////////////////
static int handle_dummy_handler(const char* s)
{
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static const struct notification_t model_mv200_notifications[] =
{
	{ .name = "$QCMTI:", .action = handle_dummy_handler },
	{0, } // zero-terminator
};

////////////////////////////////////////////////////////////////////////////////
int mv200_get_status(const struct model_status_info_t* status_needed, struct model_status_info_t* new_status,struct model_status_info_t* err_status)
{
	struct tms tmsbuf;
	clock_t clkNow=times(&tmsbuf);

	memset(new_status,0,sizeof(*new_status));

	if(status_needed->status[model_status_sim_ready])
	{
		static int fSIMReady=0;

		err_status->status[model_status_sim_ready]=0;
		if(!fSIMReady)
			fSIMReady=!(clkNow-_clkInsert<(WAITTIME_SIMREADY*_clkPerSec));
		new_status->status[model_status_sim_ready]=fSIMReady;
	}

	if(status_needed->status[model_status_registered])
	{
		static int fNwReg=0;

		err_status->status[model_status_registered]=0;
		if(!fNwReg)
			fNwReg=!(clkNow-_clkInsert<(WAITTIME_NWREG*_clkPerSec));
		new_status->status[model_status_registered]=fNwReg;
	}

	if(status_needed->status[model_status_attached])
	{
		static int fAtt=0;

		err_status->status[model_status_attached]=0;
		if(!fAtt)
			fAtt=!(clkNow-_clkInsert<(WAITTIME_ATT*_clkPerSec));
		new_status->status[model_status_attached]=fAtt;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
struct model_t model_mv200 = {
	.name = "Axess",
	.init = mv200_init,
	.detect = mv200_detect,

	.get_status = mv200_get_status,
	.set_status = mv200_set_status,

	.commands = NULL,
	.notifications = model_mv200_notifications
};

////////////////////////////////////////////////////////////////////////////////

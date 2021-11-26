/*!
* Copyright Notice:
* Copyright (C) 2012 NetComm Pty. Ltd.
*
* This file or portions thereof may not be copied or distributed in any form
* (including but not limited to printed or electronic forms and binary or object forms)
* without the expressed written consent of NetComm Wireless Pty. Ltd
* Copyright laws and International Treaties protect the contents of this file.
* Unauthorized use is prohibited.
*
*
* THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
* NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
* SUCH DAMAGE.
*
*/
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <unistd.h>
#include <alloca.h>
#include <sys/ioctl.h>

#ifndef USE_ALSA
#include <sys/select.h>
#endif

#include "slic/types.h"
#include "cdcs_utils.h"
#include "cdcs_syslog.h"
#include "../cnsmgr/voicecall_constants.h"
#include "./calls/calls.h"
#include "./daemon.h"
#include "./pots_rdb_operations.h"
#include "./slic_control/slic_control.h"
#ifndef USE_ALSA
/* slic cal data save/restore feature */
#include "slic/calibration.h"
#endif
#include "./telephony_profile.h"

#if defined(PLATFORM_PLATYPUS)
#include <nvram.h>
#endif

void initialize_sp(void)
{
	char s[20] = {0, };
	char mcc[20] = {0, };
	char mnc[20] = {0, };

	/* read triggered rdb variables */
	(void) rdb_get_single( rdb_variable( RDB_IMSI_PREFIX, "", RDB_PLMN_MCC ), mcc, 20 );
	(void) rdb_get_single( rdb_variable( RDB_IMSI_PREFIX, "", RDB_PLMN_MNC ), mnc, 20 );
	
	/* currently Rogers only need to be set */
	if (tel_profile == CANADIAN_PROFILE) {
		if( rdb_get_single("system.skin", s, 10 ) != 0)
		{
			SYSLOG_ERR( "failed to read system skin" );
			return;
		}
		if (strcmp(s, "ro") == 0)
		{
			tel_sp_profile = SP_ROGERS;
			SYSLOG_ERR( "Set service provider to Rogers" );
		}
		else if (strcmp(s, "ts") == 0)
		{
			tel_sp_profile = SP_TELUS;
			SYSLOG_ERR( "Set service provider to Telus" );
		}
	} else if (tel_profile == DEFAULT_PROFILE) {
		/* compare NZ MCC for special feature. If there is requests from
		other SP, then add MNC comparison here */
		if (strcmp(mcc, "530") == 0) {
			tel_sp_profile = SP_NZ_TELECOM;
		}
		/* Telstra with MCC and MNC for SP specific feature and behavior. */
		else if (strcmp(mcc, "505") == 0 && atoi(mnc) == 1) {
			tel_sp_profile = SP_TELSTRA;
		}
		/* Malaysian profile represented by Celcom */
		else if (strcmp(mcc, "502") == 0) {
			tel_sp_profile = SP_CELCOM;
		}
	}
}

/* initialize profile & ISP list variables */
const char *tel_profile_name[MAX_PROFILE] = { "Default(Australia, New Zealand)", "Canada", "North America" };
const char *tel_sp_name[MAX_SP_PROFILE] = { "Default", "Rogers", "Telus", "NZ Telecom", "Telstra", "Celcom" };
#define LONG_BUF_SIZE           1024
void initialize_profile_variables(void)
{
	int i;
	char buf[LONG_BUF_SIZE];

	(void) memset(buf, 0x00, LONG_BUF_SIZE);
	for (i = 0; i < MAX_PROFILE; i++)
	{
		strcat(buf, tel_profile_name[i]);
		strcat(buf, ";");
	}
	if (rdb_update_single(RDB_PROFILE_LIST, buf, CREATE, ALL_PERM, 0, 0) != 0)
	{
		SYSLOG_ERR("failed to update '%s' (%s)", RDB_PROFILE_LIST, strerror(errno));
	}

	(void) memset(buf, 0x00, LONG_BUF_SIZE);
	for (i = 0; i < MAX_SP_PROFILE; i++)
	{
		strcat(buf, tel_sp_name[i]);
		strcat(buf, ";");
	}
	if (rdb_update_single(RDB_SP_LIST, buf, CREATE, ALL_PERM, 0, 0) != 0)
	{
		SYSLOG_ERR("failed to update '%s' (%s)", RDB_SP_LIST, strerror(errno));
	}
}

#define BUF_SIZE    256
void display_pots_setting(const char* device[], const char* instance, BOOL changed)
{
	int i;
	char buf[BUF_SIZE];

	SYSLOG_INFO("======================================================================");
	SYSLOG_INFO(" Telephony Profile : %s", tel_profile_name[tel_profile]);
	SYSLOG_INFO(" Service Provider  : %s", tel_sp_name[tel_sp_profile]);
	if (!changed) {
		(void) memset(buf, 0x00, BUF_SIZE);
		for (i = 0; i < MAX_CHANNEL_NO && device[i] != 0x00 && strcmp(device[i], "") != 0; i++)
		{
			strcat(buf, device[i]);
			strcat(buf, " ");
		}
		SYSLOG_INFO(" SLIC Devices      : %s", buf);
	}
	SYSLOG_INFO("======================================================================");
}


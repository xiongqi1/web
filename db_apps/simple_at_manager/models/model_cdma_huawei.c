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
#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "../util/rdb_util.h"

#include "../rdb_names.h"
#include "../at/at.h"
#include "../model/model.h"
#include "../util/scheduled.h"
#include "../util/at_util.h"

#include "model_default.h"

#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

extern struct model_t model_default;
extern int cdma_update_signal_strength(void);
extern int cdma_update_imei(void);

///////////////////////////////////////////////////////////////////////////////
static int cdma_handle_command_band(const struct name_value_t* args)
{
	// not implemented
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int cdma_huawei_init(void)
{
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int cdma_huawei_get_status(const struct model_status_info_t* status_needed, struct model_status_info_t* new_status,struct model_status_info_t* err_status)
{
     char* resp;
	int nw_stat, token_count, ok, sim_status=255, srv_domain = 0;
	char response[AT_RESPONSE_MAX_SIZE];
	char *value;
	const char *t;

	memset(new_status,0,sizeof(*new_status));

     if (at_send("AT^SYSINFO", response, "^SYSINFO", &ok, 0) != 0 || !ok)
		return -1;

	if(strlen(response)<STRLEN("^SYSINFO:"))
		return -1;

	value = response + STRLEN("^SYSINFO:");

	token_count = tokenize_at_response(value);

	if (token_count >= 2)
	{
		t = get_token(value, 1);

		if(t)
			srv_domain=atoi(t);
	}

	if (token_count >= 5)
	{
		t = get_token(value, 4);

		if(t)
			sim_status=atoi(t);
	}

	if(status_needed->status[model_status_sim_ready])
	{
		if(sim_status == 1 || sim_status == 240) {
      		new_status->status[model_status_sim_ready]=!0;
      		err_status->status[model_status_registered]=0;
		}
	}

	if(status_needed->status[model_status_registered])
	{
		if(srv_domain >= 1 && srv_domain <= 3) {
		   new_status->status[model_status_registered]= !0;
		   err_status->status[model_status_registered]=0;
		}
	}

	if(status_needed->status[model_status_attached] && new_status->status[model_status_registered] != 0)
	{
		if( (resp=at_bufsend("AT+CSQ?",""))!=0 )
		{
			t = strtok(resp, ",");

			nw_stat=99;

			if(t)
				nw_stat=atoi(t);

			if(nw_stat > 0 && nw_stat <= 31) {
			   new_status->status[model_status_attached]= !0;
			   err_status->status[model_status_attached]=0;
			}
		}

	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
void huawei_cdma_update_module_status()
{
	char response[AT_RESPONSE_MAX_SIZE];
	char *value;
	const char *t;
	int token_count, ok;
	int srv_status=0, srv_domain=0, sys_mode=0, sim_state=255;
	const char * sysmode[] = {"No service", "", "CDMA mode", "", "HDR mode", "", "", "", "CDMA/HDR HYBRID mode"};
	const char * srvdomain[] = {"No service", "CS service", "PS service", "PS service / CS service"};


/*

   # format
   ^SYSINFO:<srv_status>,<srv_domain>,<roam_status>,<sys_mode>,<sim_state>[,<lock_state>,<sys_submode>]

   1. <srv_status>: System service status. Values are as follows:
      0: No service
      1: Restricted service
      2: Valid service
      3: Restricted domain services
      4: Power-saving mode and hibernate mode

   2. <srv_domain>: System domain. Values are as follows:
      0: No service
      1: Only Circuit Switched domain (CS) service
      2: Only Packet Switched (PS) domain service
      3: Both PS service and CS service
      4: Neither CS service nor PS service is registered and MS is in the searching mode
      255: CDMA not supported

   3. <roam_status>: Roaming status. Values are as follows:
      0: Non-roaming status
      1: Roaming status

   4. <sys_mode>: System mode. Values are as follows:
      0: No service
      2: CDMA mode
      4: HDR mode
      8: CDMA/HDR HYBRID mode

   5. <sim_state>: UIM card status. Values are as follows:
      1: Valid UIM card
      240: ROMSIM version
      255: UIM card not exist

*/

     if (at_send("AT^SYSINFO", response, "^SYSINFO", &ok, 0) != 0 || !ok)
		goto error;

	if(strlen(response)<STRLEN("^SYSINFO:"))
		goto error;

	value = response + STRLEN("^SYSINFO:");

	token_count = tokenize_at_response(value);


	if (token_count < 5)
	   goto error;

     t = get_token(value, 0);
     if(t)
         srv_status=atoi(t);

     t = get_token(value, 1);
     if(t)
         srv_domain=atoi(t);

     t = get_token(value, 3);
     if(t)
         sys_mode=atoi(t);

     t = get_token(value, 4);
     if(t)
         sim_state=atoi(t);


     if(srv_domain >=0 && srv_domain <= 3)
        rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), srvdomain[srv_domain]);
     else if(srv_domain == 255)
        rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), "CDMA not supported");
     else
        rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), "");


     if(sys_mode >=0 && sys_mode <= 8)
        rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), sysmode[sys_mode]);
     else
        rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");


     if(sim_state == 1 || sim_state == 240)
	   rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM OK");
	else
	   rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM not inserted");

     return;

error:
   rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), "");
   rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), "");
   rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM not inserted");
   return;

}
////////////////////////////////////////////////////////////////////////////////
int cdma_huawei_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{


	cdma_update_signal_strength();
	cdma_update_imei();
	huawei_cdma_update_module_status();

	return 0;
}

char *strcasestr(const char *haystack, const char *needle);

////////////////////////////////////////////////////////////////////////////////
static int cdma_huawei_detect(const char* manufacture, const char* model_name)
{
   int loop_cnt;

   struct manufacture_model_pair
   {
    char *manufacture;
    char *model;
   } manufacturetbl[] =
   {
    {"HUAWEI", "EC1705"}, //HUAWEI EC1705

    {NULL, NULL}
   };

   for(loop_cnt=0 ; manufacturetbl[loop_cnt].manufacture != NULL; loop_cnt++)
   {
      if (strstr(manufacture, manufacturetbl[loop_cnt].manufacture) && strstr(model_name,manufacturetbl[loop_cnt].model)) {
         return 1;
      }
   }

   return 0;

}

////////////////////////////////////////////////////////////////////////////////
struct command_t cdma_huawei_commands[] =
{
	{ .name = RDB_BANDCMMAND,		.action = cdma_handle_command_band },

	{0,}
};

///////////////////////////////////////////////////////////////////////////////
static int notiDummy(const char* s)
{
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
const struct notification_t cdma_huawei_notifications[] =
{
#if 0
   { .name = "+CLIP", .action = handle_clip_notification },
   { .name = "+CCWA", .action = handle_ccwa_notification },
   { .name = "RING", .action = handle_ring },
   { .name = "+CRING", .action = handle_ring },
   { .name = "CONNECT", .action = handle_connect },
   { .name = "REJECT", .action = handle_disconnect },
   { .name = "DISCONNECT", .action = handle_disconnect },
   { .name = "NO CARRIER", .action = handle_disconnect },
   { .name = "BUSY", .action = handle_disconnect },
   { .name = "+CMT:", .action = notiSMSRecv },
   { .name = "+ZUSIMR:", .action = notiDummy },
   { .name = "^BOOT:", .action = notiDummy },
   { .name = "^DSFLOWRPT:", .action = notiDummy },
#endif
   { .name = "^MODE:", .action = notiDummy },
   { .name = "^RSSILVL:", .action = notiDummy },
   { .name = "^HRSSILVL:", .action = notiDummy },
   { .name = "^CRSSI:", .action = notiDummy },
   { .name = "^HDRRSSI:", .action = notiDummy },
   {0, } // zero-terminator
};
////////////////////////////////////////////////////////////////////////////////
struct model_t model_cdma_huawei = {
	.name = "cdma_huawei",
	.init = cdma_huawei_init,
	.detect = cdma_huawei_detect,

	.get_status = cdma_huawei_get_status,
	.set_status = cdma_huawei_set_status,

//	.commands = cdma_huawei_commands,
	.commands = NULL,
	.notifications = cdma_huawei_notifications
};

////////////////////////////////////////////////////////////////////////////////
 


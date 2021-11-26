
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../at/at.h"
#include "../model/model.h"
#include "../util/rdb_util.h"
#include "../rdb_names.h"
#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "model_default.h"

extern int cdma_update_signal_strength(void);
extern void cdma_update_sim();

///////////////////////////////////////////////////////////////////////////////
static int cdma_alcatel_init(void)
{
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
static int cdma_alcatel_detect(const char* manufacture, const char* model_name)
{
   int loop_cnt;

   struct manufacture_model_pair
   {
    char *manufacture;
    char *model;
   } manufacturetbl[] =
   {
    {"QUALCOMM INCORPORATED", "119"}, //Alcatel OT-X150

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
int cdma_alcatel_get_status(const struct model_status_info_t* status_needed, struct model_status_info_t* new_status,struct model_status_info_t* err_status)
{
	char* resp;

	memset(new_status,0,sizeof(*new_status));

	if(status_needed->status[model_status_sim_ready])
	{
		// assume SIM is always okay for CDMA 
		new_status->status[model_status_sim_ready]=!0;
		err_status->status[model_status_registered]=0;
	}

	if(status_needed->status[model_status_registered])
	{
		char* stat_str;
		int nw_stat;

		if( (resp=at_bufsend("AT+CREG","+CREG: "))!=0 )
		{
			strtok(resp, ",");
			stat_str=strtok(0, ",");

			nw_stat=-1;
			if(stat_str)
				nw_stat=atoi(stat_str);

			new_status->status[model_status_registered]=(nw_stat==1) || (nw_stat==5);
		}

		err_status->status[model_status_registered]=!resp;
	}

	if(status_needed->status[model_status_attached] && new_status->status[model_status_registered] != 0)
	{
		char* stat_str;
		int nw_stat;

		if( (resp=at_bufsend("AT+CSQ?",""))!=0 )
		{
			stat_str = strtok(resp, ",");

			nw_stat=99;

			if(stat_str)
				nw_stat=atoi(stat_str);

			if(nw_stat > 0 && nw_stat <= 31) {
			   new_status->status[model_status_attached]= !0;
			   err_status->status[model_status_attached]=0;
			}
		}

	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int cdma_alcatel_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	if(!at_is_open())
		return 0;

//	update_sim_status();

	cdma_update_signal_strength();
	
	cdma_update_sim();

//	update_imsi();

//	update_network_name();
//	update_service_type();

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
struct model_t model_cdma_alcatel =
{
	.name = "cdma_Alcatel",

	.init = cdma_alcatel_init,
	.detect = cdma_alcatel_detect,

	.get_status = cdma_alcatel_get_status,
	.set_status = cdma_alcatel_set_status,

	.commands = NULL,
	.notifications = NULL
};

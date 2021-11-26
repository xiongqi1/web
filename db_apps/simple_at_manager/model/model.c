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

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "cdcs_syslog.h"

#include "rdb_ops.h"
#include "../at/at.h"
#include "./commands.h"
#include "./model.h"
#include "../rdb_names.h"
#include "../util/rdb_util.h"
#include "../util/scheduled.h"

#include "../featurehash.h"


#define DURATION_MODEL_UPDATE_STATUS 10
#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

int stripStr(char* szValue);

struct name_model_pair_t
{
	const char* name;
	struct model_t* model;
};

extern struct model_t model_4031;
extern struct model_t model_sierra;
extern struct model_t model_default;
extern struct model_t model_mv200;
extern struct model_t model_ipw;
extern struct model_t model_longsung;
extern struct model_t model_fusion;
extern struct model_t model_ericsson;
extern struct model_t model_cdma_default;
extern struct model_t model_cdma_huawei;
extern struct model_t model_cdma_alcatel;
extern struct model_t model_optionwireless;
extern struct model_t model_ipwlte;
extern struct model_t model_CyfrowyPolsat;
extern struct model_t model_ZTE_k3805z;
extern struct model_t model_mhs;
extern struct model_t model_huiwei;
extern struct model_t model_phs8p;
extern struct model_t model_bgs2e;
extern struct model_t model_pvs8;
extern struct model_t model_quanta;
extern struct model_t model_sequans;
extern struct model_t model_quectel;

extern volatile BOOL sms_disabled;

static struct model_t* model_ = NULL;

static struct name_model_pair_t models_[] =
{
#if defined(PLATFORM_PLATYPUS2) && defined(SKIN_swi)
	{ .name = "Sierra MHS", .model = &model_mhs },
#endif 
	{ .name = "Sierra", .model = &model_sierra },
	{ .name = "Qualcomm", .model = &model_4031 },
	{ .name = "AxessTel", .model = &model_mv200 },
	{ .name = "IPW", .model=&model_ipw },
	{ .name = "Longsung", .model=&model_longsung},
	{ .name = "Fusion", .model=&model_fusion},
	{ .name = "Ericsson", .model=&model_ericsson},
	{ .name = "Cdma_default", .model=&model_cdma_default},
	{ .name = "Cdma_huawei", .model=&model_cdma_huawei},
	{ .name = "Cdma_huawei", .model=&model_cdma_alcatel},
	{ .name = "Option Wireless", .model=&model_optionwireless},
	{ .name = "IP Wireless LTE", .model=&model_ipwlte},
	{ .name = "CyfrowyPolsat", .model=&model_CyfrowyPolsat},
	{ .name = "ZTE_k3805z", .model=&model_ZTE_k3805z},
	{ .name = "Huawei", .model=&model_huiwei},
	{ .name = "Cinterion", .model=&model_phs8p},
	{ .name = "Cinterion2G", .model=&model_bgs2e},
	{ .name = "CinterionCDMA", .model=&model_pvs8},
	{ .name = "Quanta", .model=&model_quanta},
	{ .name = "SEQUANS", .model=&model_sequans},
	{ .name = "Quectel", .model=&model_quectel},
};

/*
	struct of table to create RDB command to Feature relationship
*/
struct t_rdb_to_feature {
	const char* rdb;
	const char* feature;
};

/*
	struct of table to create RDB command to Feature relationship
*/
struct t_rdb_to_feature rdb_to_feature[]={
	{RDB_PROFILE_CMD,FEATUREHASH_CMD_CONNECT},
	{RDB_SIMCMMAND,FEATUREHASH_CMD_SIMCARD},
	{RDB_GPS_PREFIX".0."RDB_GPS_CMD,FEATUREHASH_CMD_GPS},
	{RDB_SMS_CMD,FEATUREHASH_CMD_SMS},
	{RDB_BANDCMMAND, FEATUREHASH_CMD_BANDSEL},
	{NULL,NULL},
};



static struct model_t* model_find(const char* manual_model,const char* manufacture, const char* model_name)
{
	size_t i;
	size_t size = sizeof(models_) / sizeof(struct name_model_pair_t);

	for (i = 0; i < size; ++i)
	{
		struct model_t* model = models_[i].model;

		SYSLOG_DEBUG("call detecting... %s",model->name);
		if(manual_model)
		{
			if(!strcmp(manual_model,models_[i].name))
			{
				SYSLOG_DEBUG("manually selected mi=%s,mm=%s ==> %s", manufacture,model_name, model->name);
				return model;
			}
		}
		else if(model->detect(manufacture,model_name))
		{
			SYSLOG_DEBUG("found phone model mi=%s,mm=%s ==> %s", manufacture,model_name, model->name);
			return model;
		}
	}

	if(manual_model)
		SYSLOG_DEBUG("manual mode = (%s)", manual_model);
	SYSLOG_DEBUG("unknown phone model mi=%s,mm=%s ==> %s", manufacture,model_name, model_default.name);
	
	return &model_default;
}

static int get_module_data(const char* command, const char* name, char* value, int fNumeric)
{
	int ok = 0;
	int attempts = 3;

	char prefix[128];
	int prefixCnt;
	char resp[ AT_RESPONSE_MAX_SIZE ];
	char *respP = &resp[0];

	while (attempts--)
	{
		if (at_send(command, respP, "", &ok, 0) == 0 && ok)
		{
			break;
		}
		if (attempts)
		{
			continue;
		}
		SYSLOG_ERR("'%s' failed", command);
		return -1;
	}

	if(fNumeric)
		stripStr(respP);

	// process for stupid prefixed non-standard AT result
	if(!strncasecmp(command,"AT+",3)) {
		snprintf(prefix,sizeof(prefix),"%s:",command+2);
		prefix[sizeof(prefix)-1]=0;

		prefixCnt=strlen(prefix);
		if( strlen(resp)>prefixCnt )
		{
			if(!strncasecmp(respP,prefix,prefixCnt))
			{
				respP+=prefixCnt;

				// skip space
				while(*respP==' ')
					respP++;
			}
		}
	}
	(void) strcpy(value, respP);
	if (rdb_set_single(rdb_name(name, ""), value) == 0)
	{
		return 0;
	}
	SYSLOG_ERR("failed to set '%s' to '%s' (%s)", rdb_name(name, ""), value, strerror(errno));
	return -1;
}

/* Caution : model_physinit() can be called multiple times so model_default.init() or
 * 			 model_->init() should be carefully written considering reenterance */
int model_physinit(void)
{
	if (model_ != &model_default && model_default.init && model_default.init() != 0)
	{
		return -1;
	}

	if (model_->init && model_->init() != 0)
	{
		return -1;
	}

	return 0;
}

void update_heartBeat()
{
	static long long int heartBeat=0;
	char heartBeatStr[128];
	
	// set heartbeat
	heartBeat++;
	sprintf(heartBeatStr,"%lld",heartBeat);
	rdb_set_single(rdb_name(RDB_HEART_BEAT, ""), heartBeatStr);
	rdb_set_single(rdb_name(RDB_HEART_BEAT2, ""), heartBeatStr);
}

////////////////////////////////////////////////////////////////////////////////
static void model_update_status_on_schedule(void* ref)
{

	static struct model_status_info_t cur_status={{0,},};

	struct model_status_info_t status_needed;

	struct model_status_info_t chg_status;
	struct model_status_info_t new_status;
	struct model_status_info_t err_status;

	static struct model_status_info_t rdb_status={{0,},};
	const char* status_str[model_status_count]={
		RDB_NETWORKSIMREADY,
		RDB_NETWORKREGISTERED,
		RDB_NETWORKATTACHED
	};

	int i;

	update_heartBeat();
	
	if(ref)
		scheduled_func_schedule("model_update_status_on_schedule",model_update_status_on_schedule,DURATION_MODEL_UPDATE_STATUS);

	// get status
	memset(&status_needed,1,sizeof(status_needed));
	memcpy(&new_status,&cur_status,sizeof(cur_status));
	memset(&err_status,1,sizeof(err_status));
	if(model_->get_status(&status_needed,&new_status,&err_status))
	{
		SYSLOG_ERR("failed to get status");
		return;
	}

	int any_chg=0;
	if(!ref)
	{
		memset(&chg_status,1,sizeof(chg_status));
	}
	else
	{
		for(i=0;i<model_status_count;i++)
		{
			if(rdb_get_single_int(rdb_name(status_str[i], ""), &cur_status.status[i]) < 0)
				cur_status.status[i]=0;

			chg_status.status[i] = (cur_status.status[i] && !new_status.status[i]) || (!cur_status.status[i] && new_status.status[i]);
			any_chg = any_chg || chg_status.status[i];
		}
	}

	// do not any network registered AT commands for the first manual lap - save time for pots_bridge - about 14 seconds
	if(!ref)
		new_status.status[model_status_registered]=0;
		
	model_->set_status(&chg_status,&new_status,&err_status);

	/* set database status varialbes */
	if( any_chg || !ref)
	{
		for(i=0;i<model_status_count;i++)
		{
			rdb_status.status[i]= 1;
		}
	}

	for(i=0;i<model_status_count;i++) {
		if(rdb_status.status[i]) {
			/* do not change attached rdb if simple at port manager is not configured for the feature */
			if(model_status_attached==i && !is_enabled_feature(FEATUREHASH_CMD_SERVING_SYS))
				continue;

			if(rdb_set_single_int(rdb_name(status_str[i], ""), new_status.status[i])>=0) {
				rdb_status.status[i]=0;
			}
			else {
				syslog(LOG_ERR,"failed to update rdb: [%s] - %s", rdb_name(status_str[i], ""),strerror(errno));
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
static void model_check_dtmf_keys_on_schedule(void* ref)
{
	scheduled_func_schedule("model_check_dtmf_keys_on_schedule",model_check_dtmf_keys_on_schedule,1);
	polling_dtmf_rdb_event();
}

////////////////////////////////////////////////////////////////////////////////
void model_check_rx_sms_on_schedule(void* ref)
{
    polling_rx_sms_event();
    scheduled_func_schedule("model_check_rx_sms_on_schedule",model_check_rx_sms_on_schedule,2);
}

int update_imei_from_ati()
{
	char resp[AT_RESPONSE_MAX_SIZE];
	int ok;
	int retry=3;
	int succ;

	char* imei;
	char* p;
	char* p2;
	
	/* get ATI */
	succ=0;
	while (retry--)
		succ=(at_send("ATI", resp, "", &ok, 0)==0) && ok;

	/* bypass if it fails */
	if(!succ) {
		SYSLOG_ERR("failed to get response from ATI");
		goto err;
	}

	/*
		ati
		Manufacturer: Sierra Wireless, Incorporated
		Model: MC7354
		Revision: SWI9X15C_05.05.16.03 r22385 carmd-fwbuild1 2014/06/04 15:01:26
		MEID: A100003DB252CE
		ESN: 12812169004, 80B9AF2C
		IMEI: 359225050024702
		IMEI SV: 13
		FSN: J8341600150310
		+GCAP: +CIS707-A, CIS-856, CIS-856-A, +CGSM, +CLTE2, +MS, +ES, +DS, +FCLASS


		OK
	*/
	p=strstr(resp,"IMEI: ");
	if(!p) {
		SYSLOG_ERR("IMEI does not exist in the response of ATI");
		goto err;
	}

	p+=STRLEN("IMEI: ");
	p2=strchr(p,'\n');
	if(!p2) {
		SYSLOG_ERR("incorrect format of ATI response");
		goto err;
	}

	imei=strndupa(p,p2-p);
	rdb_set_single(rdb_name(RDB_IMEI, ""), imei);

	return 0;

err:
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
int is_ericsson = 0;
int is_cinterion = 0;
int is_cinterion_cdma = 0;
int model_init(const char* model)
{
	char value[ AT_RESPONSE_MAX_SIZE ];

	char mi[AT_RESPONSE_MAX_SIZE];
	char mm[AT_RESPONSE_MAX_SIZE];

	/* get manufacture */
	if ( (get_module_data("AT+CGMI", RDB_MANUFACTURER, mi,0) < 0) && 
	     (get_module_data("AT$CGMI?", RDB_MANUFACTURER, mi,0) < 0) && 
	     (get_module_data("AT+GMI", RDB_MANUFACTURER, mi,0) < 0))
	{
		SYSLOG_ERR("CGMI failed");
		return -1;
	}

	/* get model */
	if ( (get_module_data("AT+CGMM", RDB_MODEL, mm,0) < 0) && 
	     (get_module_data("AT$CGMM?", RDB_MODEL, mm,0) < 0) && 
	     (get_module_data("AT+GMM", RDB_MODEL, mm,0) < 0))
	{
		SYSLOG_ERR("CGMM failed");
		return -1;
	}

	
	/* get IMEI */
	if (
		#if defined(MODULE_MC7354) || defined(MODULE_MC7304)
		/* IMEI for Sierra MC7354 - AT+GSN does not work for MC7354 */
		(update_imei_from_ati()<0) &&
		#endif
		(get_module_data("AT+CGSN", RDB_IMEI, value,1) < 0) &&
		(get_module_data("AT$CGSN?", RDB_IMEI, value,1) < 0) &&
		(get_module_data("AT+GSN", RDB_IMEI, value,1) < 0))
	{
		if (rdb_set_single(rdb_name(RDB_IMEI, ""), "N/A") != 0)
		{
			return -1;
		}
	}

	/* get firmware */
	if ( (get_module_data("AT+CGMR", RDB_FIRMWARE_VERSION, value,0) < 0) && 
	     (get_module_data("AT$CGMR?", RDB_FIRMWARE_VERSION, value,0) < 0) && 
	     (get_module_data("AT+GMR", RDB_FIRMWARE_VERSION, value,0) < 0))
	{
		return -1;
	}

	SYSLOG_DEBUG("mi=%s ,mm=%s",mi,mm);
	/* get model */
	model_ = model_find(model,mi,mm);
	(void) strcpy(&model_->variants[0], mm);

	is_ericsson = (strcmp(model_name(),"Ericsson") == 0);
	is_cinterion = (strcmp(model_name(),"cinterion") == 0 || strcmp(model_name(),"cinterion2G") == 0);
	is_cinterion_cdma = (strcmp(model_name(),"cinterion_cdma") == 0);

	if (is_ericsson) {
		SYSLOG_DEBUG("Found Ericsson module which need special process for SMS/USSD etcs,.\n");
	}

	/* physical initialize */
	if (model_physinit() < 0)
		return -1;

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int model_init_schedule(void)
{
	/* call initial update */
	model_update_status_on_schedule(0);
	
#if defined(BOARD_nhd1w)
	syslog(LOG_ERR,"SMS disabled");
	scheduled_func_schedule("model_update_status_on_schedule",model_update_status_on_schedule,DURATION_MODEL_UPDATE_STATUS);
#else	
	scheduled_func_schedule("model_update_status_on_schedule",model_update_status_on_schedule,DURATION_MODEL_UPDATE_STATUS);

	/* dtmf key check scheduler */
	scheduled_func_schedule("model_check_dtmf_keys_on_schedule",model_check_dtmf_keys_on_schedule,1);

	if(!sms_disabled) {
		/* rx sms check scheduler */
        model_check_rx_sms_on_schedule(0);
		//scheduled_func_schedule("model_check_rx_sms_on_schedule",model_check_rx_sms_on_schedule,2);
	}
	
#endif	
	
	return 0;
}


static int model_notify_impl(struct model_t* m, const char* notification)
{
	int matched;

	//SYSLOG_DEBUG("model_notify_impl: enter\n");

	size_t i = 0;
	for (i = 0; m->notifications[i].name; ++i)
	{
		//SYSLOG_DEBUG("model_notify_impl: looking up - noti name = %s", m->notifications[i].name);

		matched = !memcmp(m->notifications[i].name, notification, strlen(m->notifications[i].name));
		if (matched) {
			SYSLOG_DEBUG("found noti: %s", m->notifications[i].name);
			return m->notifications[i].action(notification);
		}
	}

	if (m != &model_default)
	{
		return model_notify_impl(&model_default, notification);
	}
	return 0;
}

int model_notify(const char* notification)
{
	if(!model_)
		return 0;

	if (model_->notifications)
	{
		return model_notify_impl(model_, notification);
	}

	if (model_ != &model_default)
	{
		return model_notify_impl(&model_default, notification);
	}

	return 0;
}

static int model_run_command_impl(struct model_t* m, const struct name_value_t* args,int* ignored)
{
	size_t i = 0;
	const struct name_value_t* command = args;
	
	if(ignored)
		*ignored=0;

	if (args == NULL || args[0].name == NULL || args[0].value == NULL)
		return 0;
	
	if (m->commands)
	{
		for (i = 0; m->commands[i].name; ++i)
		{
			struct t_rdb_to_feature* p=rdb_to_feature;

			// ignore if the feature is disabled
			while(p->rdb && p->feature) {
				if(!strcmp(command->name,p->rdb) && !is_enabled_feature(p->feature)) {
					if(ignored)
						*ignored=1;
					
					SYSLOG_DEBUG("%s: command profile feature not enabled - %s",__FUNCTION__,FEATUREHASH_CMD_CONNECT);
					return 0;
				}

				p++;
			}

			if (strcmp(m->commands[i].name, command->name) == 0)
			{
				SYSLOG_INFO("%s handling '%s'='%s'", m->name, args[0].name, args[0].value);
				int ret;
				ret = m->commands[i].action(args);
				rdb_set_single(rdb_name(m->commands[i].name, "status"), ret == 0 ? "1" : "0");     // TODO: use return channel instead? (otherwise the receiver may miss the result)
				/* do not clear log mask value after exiting from this function */
				if (strcmp(m->commands[i].name, RDB_SYSLOG_PREFIX"."RDB_SYSLOG_MASK) == 0) {
					if(ignored)
						*ignored=1;
				}

				return ret;
			}
		}

		if (m == &model_default)
		{
			SYSLOG_INFO("%s does not know how to handle '%s'='%s'", m->name, args[0].name, args[0].value);
		}
	}

	return m == &model_default ? 0 : model_run_command_impl(&model_default, args, ignored);
}

int model_run_command(const struct name_value_t* args,int* ignored)
{
	return model_run_command_impl(model_, args,ignored);
}

int model_run_command_default(const struct name_value_t* args)
{
	return model_run_command_impl(&model_default, args,0);
}

const struct model_t* get_model_instance()
{
	return model_;
}

const char* model_name(void)
{
    /* return dummy before model_->name is set to prevent error */
    static const char dummy_model[]="dummy";
    return (model_ ? model_->name : (const char *)&dummy_model);
}

const char* model_variants_name(void)
{
    /* return dummy before model_->variants is set to prevent error */
    static const char dummy_variants[]="dummy";
    return (model_ ? model_->variants : (const char *)&dummy_variants);
}

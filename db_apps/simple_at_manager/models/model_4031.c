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
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include "rdb_ops.h"
#include "cdcs_syslog.h"

#include "../util/rdb_util.h"
#include "../rdb_names.h"
#include "../at/at.h"
#include "../model/model.h"
#include "model_default.h"

#include "../util/scheduled.h"
#include "../util/at_util.h"

#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

extern struct model_t model_default;

static int m4031_csq_notification(const char* s)
{
	int value;
	char dbm[RDB_COMMAND_MAX_LEN];

	SYSLOG_DEBUG("m4031_csq_notification: enter\n");

	value = atoi(s + STRLEN("*CSQ: "));
	sprintf(dbm, "%ddBm",  value < 0 || value == 99 ? 0 : (value * 2 - 113));

	SYSLOG_INFO("%s = %s", rdb_name(RDB_SIGNALSTRENGTH, ""), dbm);

	return rdb_set_single(rdb_name(RDB_SIGNALSTRENGTH, ""), dbm);
}

static int m4031_qcnr_notification(const char* s)
{
	const char* msgbody;
	char service_info[RDB_COMMAND_MAX_LEN];
	char service_domain[RDB_COMMAND_MAX_LEN];
	char using[RDB_COMMAND_MAX_LEN];

	char* bandptr = service_info;
	char* bracketptr = using;
	char* commaptr = service_domain;

	int inbracket = 0;
	int comma = 0;

	SYSLOG_DEBUG("m4031_qcnr_notification: enter\n");

	msgbody = s + strlen("*QCNR: ");
	while (*msgbody)
	{
		if (*msgbody == '[')
			inbracket = 1;

		if (!inbracket && *msgbody == ',')
		{
			comma = 1;
		}
		else
		{
			if (comma)
				*commaptr++ = *msgbody;
			else if (inbracket)
				*bracketptr++ = *msgbody;
			else
				*bandptr++ = *msgbody;

			if (*msgbody == ']')
				inbracket = 0;
		}

		msgbody++;
	}

	*bandptr = 0;
	*bracketptr = 0;
	*commaptr = 0;

	// set service type

	rdb_set_single(rdb_name(RDB_SERVICETYPE, ""), service_info);
	rdb_set_single(rdb_name(RDB_CURRENTBAND, ""), service_domain);

	SYSLOG_INFO("%s = %s\n", rdb_name(RDB_SERVICETYPE, ""), service_domain);
	SYSLOG_INFO("%s = %s\n", rdb_name(RDB_CURRENTBAND, ""), service_info);

	return 0;
}


static int q4031_update_network_type(void)
{
	static const char* service_types[] = { "GSM", "GSM Compact", "UMTS", "EGPRS", "HSDPA", "HSUPA", "HSDPA/HSUPA", "E-UMTS", NULL };
	int i;
	int ok = 0;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	char buf[32];
//char buf[]={0x01,0x5a,0xf6,0x5b,0xfb,0x96,0xe1,0x4f,0};
	char network_name[128];
	char unencoded_network_name[128];
	int token_count;
	int service_type = 0;
	//char band_list_name[256];

	if (at_send_with_timeout("AT+COPS?", response, "+COPS", &ok, AT_QUICK_COP_TIMEOUT, 0) != 0 || !ok)
	{
		return -1;
	}
	network_name[0] = 0;
	unencoded_network_name[0] = 0;
	buf[0] = 0;
	token_count = tokenize_at_response(response);
	if (token_count >= 4)
	{
		const char* t = get_token(response, 3);
		int offset = *t == '"' ? 1 : 0;
		int size = strlen(t) - offset * 2;
		size = size < 63 ? size : 63; // quick and dirty
		memcpy(buf, t + offset, size);
		buf[size] = 0;
        /* replace "Telstra Mobile" to "Telstra" */
        str_replace(&buf[0], "Telstra Mobile", "Telstra");
        /* replace "3Telstra" to "Telstra" */
        str_replace(&buf[0], "3Telstra", "Telstra");
		for(i=0; i<strlen(buf); i++)
		{
			sprintf( network_name+strlen(network_name), "%%%02x", buf[i] );
			sprintf( unencoded_network_name+strlen(unencoded_network_name), "%c", buf[i] );
		}
		t = get_token(response, 1);
		rdb_set_single(rdb_name(RDB_PLMNMODE, ""), t);
		if (token_count > 4)
		{
			service_type = atoi(get_token(response, 4));
			if (service_type < 0 || service_type >= sizeof(service_types) / sizeof(char*))
			{
				SYSLOG_ERR("expected service type from 0 to %d, got %d", sizeof(service_types) / sizeof(char*), service_type);
				service_type = 0;
			}
			else
			{
				rdb_set_single(rdb_name(RDB_PLMNSYSMODE, ""), service_types[service_type]);
			}
		}
	}
	rdb_set_single(rdb_name(RDB_NETWORKNAME, ""), network_name);
	rdb_update_single(rdb_name(RDB_NETWORKNAME, "unencoded"), unencoded_network_name, CREATE, ALL_PERM, 0, 0);
	return 0;
}

static int checkband(char *response)
{
	int ok = 0;
	char *pos;
	if (at_send("AT+CBAND?", response, "", &ok, 0) != 0 || !ok)
	{
		rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "error");
		rdb_set_single(rdb_name(RDB_BANDPARAM, ""), "ANY");
		return -1;
	}
	if (strstr(response, "ANY"))
	{
		rdb_set_single(rdb_name(RDB_BANDPARAM, ""), "ANY");
		rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "0");
		return 0;
	}
/*	if (at_send("AT$CGBAND?", response, "", &ok, 0) != 0 || !ok || strlen(response) < 10)
	{
		rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "error");
		rdb_set_single(rdb_name(RDB_BANDPARAM, ""), "ANY");
		return -1;
	}*/
	for (pos = response; *pos; pos++)
	{
		if (*pos == '\"') *pos = ' ';
	}
	rdb_set_single(rdb_name(RDB_BANDPARAM, ""), response + 8);
	rdb_set_single(rdb_name(RDB_BANDSTATUS, ""), "0");
	return 0;
}

//static const char* CGBANDlist[] = { "GSM_850", "GSM_EGSM_900", "GSM_DCS_1800", "GSM_PCS_1900", "WCDMA_VI_800", "WCDMA_V_850", "WCDMA_VIII_900", "WCDMA_II_PCS_1900", "WCDMA_I_IMT_2000", "W2100", "\0" };
static int handle_setband(const struct name_value_t* args)
{
	int ok = 0;
	char command[32];
	char response[AT_RESPONSE_MAX_SIZE];
	char buf[128];

	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		return -1;
	}

	if (memcmp(args[0].value, "set", STRLEN("set")) == 0)
	{
		if (rdb_get_single(rdb_name(RDB_BANDPARAM, ""), buf, sizeof(buf)) != 0)
		{
			SYSLOG_ERR("failed to get '%s' (%s)", rdb_name(RDB_BANDPARAM, ""), strerror(errno));
			return -1;
		}

		sprintf(command, "AT+CBAND=\"%s\"", buf);
		if (at_send(command, response, "", &ok, 0) != 0 || !ok)
		{
			;
		}
		return checkband(response);
	}
	else if (memcmp(args[0].value, "check", STRLEN("check")) == 0)
	{
		return checkband(response);
	}
	SYSLOG_ERR("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
	return -1;
}

int check_pin_status(char *response)
{
	int ok = 0;
	char *pos;
	char buf[16];
	int token_count;
	const char* sim_status;

	at_send("AT", response, "OK", &ok, 0);
	at_send("AT$QPVRF=\"P1\"", response, "", &ok, 0);
	if (strstr(response, "ERROR"))
	{
		rdb_set_single(rdb_name(RDB_SIMRETRIES, ""), "Unknow");
	}
	else if (strncmp(response, "$QPVRF: ", 8) == 0)
	{
		sprintf(buf, "%d", atoi(response + 8));
		rdb_set_single(rdb_name(RDB_SIMRETRIES, ""), buf);
		pos = strchr(response, ',');
		if (pos)
		{
			if (*(++pos) == '1')
				rdb_set_single(rdb_name(RDB_SIMPINENABLED, ""), "Disabled");
			else
				rdb_set_single(rdb_name(RDB_SIMPINENABLED, ""), "Enabled");
		}
	}
	at_send("AT", response, "OK", &ok, 0);
	if (at_send("AT+CPIN?", response, "+CPIN", &ok, 0) != 0)
		return 0;

	if(!ok)
	{
		rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM not inserted");
		return 0;
	}

	token_count = tokenize_at_response(response);
	if (token_count < 2)
	{
		SYSLOG_ERR("invalid response from AT");
		return -1;
	}
	sim_status = get_token(response, 1);
	if (strcmp(sim_status, "SIM PIN") == 0)
	{
		rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM PIN Required");
		return 0;
	}
	else if (strcmp(sim_status, "READY") == 0)
	{
		rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM OK");
		return 0;
	}
	rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), sim_status);
	SYSLOG_ERR("handling sim status '%s' not implemented!", sim_status);
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
extern int Hex( char  hex);

static int q4031_update_sim(void)
{
	char response[AT_RESPONSE_MAX_SIZE];
	char value[32];
	int ok;
	char *pos1, *pos2;
	int stat, i;

	update_sim_hint();

	stat=at_send("AT+CRSM=176,12258,0,0,10", response, "", &ok, 0);
	if ( !stat && ok) {
		pos1=strstr(response, "+CRSM:");
		if(pos1) {
			pos2=strstr(pos1, ",\"");
			if( pos2 && strlen(pos2)>5 ) {
				pos1=strchr( pos2+2, '\"');
				if(pos1) {
					*pos1=0;
					pos1=pos2+2;
					for( i=0; i<strlen(pos2)-2; i+=2) {
						value[i]=*(pos1+i+1);
						if(*(pos1+i)=='F') *(pos1+i)=0;
						value[i+1]=*(pos1+i);
					}
					value[i]=0;
					rdb_set_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), value);
				}
			}
		}
	}
	return check_pin_status(response);
}

static int handle_simcommands(const struct name_value_t* args)
{
	int ok = 0;
	char command[32];
	char response[AT_RESPONSE_MAX_SIZE];
	char buf[128];
	char newpin[16];
	char simpuk[16];
	if (!args || !args[0].value)
	{
		SYSLOG_ERR("expected command, got NULL");
		return -1;
	}

	if (at_send("AT+CPIN?", response, "", &ok, 0) != 0 || !ok)
	{
		rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), "AT+CPIN?  has failed");
		return -1;
	}
//printf("handle_simcommands:%s\n",args[0].value);
	if (memcmp(args[0].value, "checkpin", STRLEN("checkpin")) == 0)
	{
		if (check_pin_status(response) == 0)
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), "Operation succeeded");
		else
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), "checkpin has failed");
		return 0;
	}
	else if (memcmp(args[0].value, "verifypuk", STRLEN("verifypuk")) == 0)
	{
//printf("verifypuk\n");
		if (rdb_get_single(rdb_name(RDB_SIMNEWPIN, ""), newpin, sizeof(newpin)) != 0)
		{
			sprintf(buf, "failed to get %s", rdb_name(RDB_SIMNEWPIN, ""));
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), buf);
			return -1;
		}
		if (rdb_get_single(rdb_name(RDB_SIMPUK, ""), simpuk, sizeof(simpuk)) != 0)
		{
			sprintf(buf, "failed to get %s", rdb_name(RDB_SIMPUK, ""));
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), buf);
			return -1;
		}
		sprintf(command, "AT+CPIN=\"%s\",\"%s\"", simpuk, newpin);
		if (at_send(command, response, "", &ok, 0) != 0 || !ok)
		{
			sprintf(buf, "%s has failed", command);
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), buf);
			return check_pin_status(response);
		}
		/*SYSLOG_ERR("verifypuk:%s",response);
		if( strstr( response, "OK" ) )
			rdb_set_single( rdb_name( RDB_SIMCMDSTATUS, ""), "Operation succeeded");
		else
			rdb_set_single( rdb_name( RDB_SIMCMDSTATUS, ""), "verifypuk has failed");*/
		sleep(3);
		return check_pin_status(response);
	}
	if (rdb_get_single(rdb_name(RDB_SIMPARAM, ""), buf, sizeof(buf)) != 0)
	{
		sprintf(buf, "failed to get %s", rdb_name(RDB_SIMPARAM, ""));
		rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), buf);
		return -1;
	}
	if ((*buf == 0) && (rdb_get_single(rdb_name(RDB_SIMAUTOPIN, ""), command, sizeof(command)) == 0)) //check autopin
	{
		if (*command == '1')
		{
			rdb_get_single(rdb_name(RDB_SIMPIN, ""), buf, sizeof(buf));
		}
	}
	if (memcmp(args[0].value, "verifypin", STRLEN("verifypin")) == 0)
	{
		sprintf(command, "AT+CPIN=\"%s\"", buf);
//printf("verifypin1:%s\n",command);
		if (at_send(command, response, "OK", &ok, 0) != 0 || !ok)
		{
			sprintf(buf, "%s has failed", command);
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), buf);
			strcpy((char *)&last_failed_pin, buf);
			//return -1;
		} else {
			(void) memset((char *)&last_failed_pin, 0x00, 16);
		}
		sleep(3);
		return check_pin_status(response);
	}
	else if (memcmp(args[0].value, "changepin", STRLEN("changepin")) == 0)
	{
		if (rdb_get_single(rdb_name(RDB_SIMNEWPIN, ""), newpin, sizeof(newpin)) != 0)
		{
			sprintf(buf, "failed to get %s", rdb_name(RDB_SIMNEWPIN, ""));
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), buf);
			return -1;
		}
		sprintf(command, "AT+CPWD=\"SC\",\"%s\",\"%s\"", buf, newpin);
		if (at_send(command, response, "", &ok, 0) != 0 || !ok)
		{
			sprintf(buf, "%s has failed", command);
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), buf);
			return -1;
		}
		if (strstr(response, "OK"))
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), "Operation succeeded");
		else
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), "changepin has failed");
		return 0;
	}
	else if (memcmp(args[0].value, "enablepin", STRLEN("enablepin")) == 0)
	{
		sprintf(command, "AT+CLCK=\"SC\",1,\"%s\"", buf);
		if (at_send(command, response, "OK", &ok, 0) != 0 || !ok)
		{
			sprintf(buf, "%s has failed", command);
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), buf);
			return -1;
		}
		rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), "Operation succeeded");
		return check_pin_status(response);
	}
	else if (memcmp(args[0].value, "disablepin", STRLEN("disablepin")) == 0)
	{
		sprintf(command, "AT+CLCK=\"SC\",0,\"%s\"", buf);
		if (at_send(command, response, "OK", &ok, 0) != 0 || !ok)
		{
			sprintf(buf, "%s has failed", command);
			rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), buf);
			return -1;
		}
		rdb_set_single(rdb_name(RDB_SIMCMDSTATUS, ""), "Operation succeeded");
		return check_pin_status(response);
	}
	SYSLOG_ERR("don't know how to handle '%s':'%s'", args[0].name, args[0].value);
	return -1;
}

static int update_setband(void)
{
int ok = 0;
char buf[128];
char command[64];
char response[AT_RESPONSE_MAX_SIZE];
	if (!rdb_get_single(rdb_name(RDB_BANDCURSEL, ""), buf, sizeof(buf)) && *buf)
	{
		sprintf(command, "AT+CBAND=\"%s\"", buf);
		if (at_send(command, response, "", &ok, 0) != 0 || !ok)
		{
			;
		}
		return checkband(response);
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int q4031_update_pdp_status()
{
	#define AT_SCACT_TIMEOUT	30

	char response[AT_RESPONSE_MAX_SIZE];
	int ok;
	int stat;

	int prefixLen;
	char* pdpStat;
	int pdpUp;

	stat=at_send_with_timeout("AT$QGACT",response,"",&ok,AT_SCACT_TIMEOUT,0);
	if(stat<0 || !ok)
		goto err;

	// check prefix
	prefixLen=sizeof("$QGACT:");
	if(strlen(response)<prefixLen+1)
		goto err;

	// get pdp status
	pdpStat=response+prefixLen;
	if(*pdpStat!='1' &&  *pdpStat!='0')
		goto err;

	pdpUp=atoi(pdpStat);
	rdb_set_single(rdb_name(RDB_PDP0STAT, ""), pdpUp==0?"down":"up");

	return 0;

err:
	return -1;
}
////////////////////////////////////////////////////////////////////////////////
static int q4031_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	q4031_update_pdp_status();

	update_configuration_setting(new_status);

	q4031_update_sim();

	update_sim_status();

	update_imsi();

	update_call_msisdn();

	update_network_name();
	q4031_update_network_type();
	update_setband();

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int q4031_detect(const char* manufacture, const char* model_name)
{
	return !strcmp(model_name,"4031");
}

////////////////////////////////////////////////////////////////////////////////
static const struct notification_t m4031_notifications[] =
{
	{ .name = "*QCNR:", .action = m4031_qcnr_notification }
	                              , { .name = "*CSQ:", .action = m4031_csq_notification }
	                              , {0,} // zero-terminator
                              };

////////////////////////////////////////////////////////////////////////////////
static const struct command_t m4031_commands[] =
{
        { .name = RDB_BANDCMMAND,	.action = handle_setband },
        { .name = RDB_SIMCMMAND,	.action = handle_simcommands },
        {0,} // zero-terminator
};


////////////////////////////////////////////////////////////////////////////////
static int q4031_init(void)
{
	update_signal_strength();
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
struct model_t model_4031 = {
	.name = "4031",

	.init = q4031_init,
	.detect = q4031_detect,

	.get_status = model_default_get_status,
	.set_status = q4031_set_status,

	.commands = m4031_commands,
	.notifications = m4031_notifications


};

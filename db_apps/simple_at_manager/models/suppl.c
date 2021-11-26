#include "suppl.h"

#include <stdio.h>
#include <alloca.h>
#include <string.h>
#include <stdlib.h>

#include "tagrule.h"
#include "../rdb_names.h"
#include "../at/at.h"
#include "../util/rdb_util.h"
#include "rdb_ops.h"
#include "../dyna.h"
#include "cdcs_syslog.h"

////////////////////////////////////////////////////////////////////////////////
#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )

#define DB_SUPPL_PREFIX	"suppl_rule"
#define DB_SUFIX_ACT    "activate"
#define DB_SUFIX_DEACT	"deactivate"
#define DB_SUFIX_QUERY	"query"

#define SUPPLE_MAX_NUMBER	128

int isNetworkTestMode();

////////////////////////////////////////////////////////////////////////////////

enum suppl_serv_type {
	suppl_serv_type_none,
	suppl_serv_type_cfi,
	suppl_serv_type_cfb,
	suppl_serv_type_cfnr,
	suppl_serv_type_cfntr,
	suppl_serv_type_clip,
	suppl_serv_type_clip2,
	suppl_serv_type_clir,
	suppl_serv_type_clir2,
	suppl_serv_type_cw,
	suppl_serv_type_cbi
};

struct suppl_serv {

	enum suppl_serv_type servType;

	const char* szServ;

	void* lpfnHandler;

	const char* szActivate;
	const char* szDeactivate;
	const char* szQuery;

	struct tagrule* pRuleActivate;
	struct tagrule* pRuleDeActivate;
	struct tagrule* pRuleQuery;

	int fEnabled;
};

enum service_action_type {
	service_action_activate,
	service_action_deactivate,
	service_action_query
};

struct supplementary_function_argument {
	struct suppl_serv* pServ;
	const char* szNumber;
	const char* szPin;
	int nTimer;
	int nBS;
};

typedef int (*suppl_func)(enum service_action_type actionType,struct supplementary_function_argument* pArg);

////////////////////////////////////////////////////////////////////////////////

typedef enum { ccfc_international = 145, ccfc_local = 129 } ccfc_type_enum;

typedef enum {
	at_cmd_class_voice = 1,
	at_cmd_class_data = 2,
	at_cmd_class_fax=4,
	at_cmd_class_sms=8,
	at_cmd_class_dcs=16,
	at_cmd_class_dca=32,
	at_cmd_class_dpa=64,
	at_cmd_class_pad=128,
	at_cmd_class_all=255,
	at_cmd_class_none=0
} at_cmd_class_enum;

////////////////////////////////////////////////////////////////////////////////
static int shCallForward(enum service_action_type actionType,struct supplementary_function_argument* pArg);
static int shClip(enum service_action_type actionType,struct supplementary_function_argument* pArg);
static int shClir(enum service_action_type actionType,struct supplementary_function_argument* pArg);
static int shCallWaiting(enum service_action_type actionType,struct supplementary_function_argument* pArg);
static int shCallBarringInternational(enum service_action_type actionType,struct supplementary_function_argument* pArg);
static int shClipClirPerCall(enum service_action_type actionType,struct supplementary_function_argument* pArg);

struct suppl_serv _supplServ[]={
	{suppl_serv_type_cfi,  "call_forward_immediate",     shCallForward,               "^\\*21\\*<number:[0-9]+>#$",                     "^#21#$",                   "^\\*#21#$",0,},
	{suppl_serv_type_cfb,  "call_forward_busy",          shCallForward,               "^\\*24\\*<number:[0-9]+>#$",                     "^#24#$",                   "^\\*#24#$",0,},
	{suppl_serv_type_cfb,  "call_forward_busy2",         shCallForward,               "^\\*67\\*<number:[0-9]+>#$",                     "^#67#$",                   "^\\*#67#$",0,},
	{suppl_serv_type_cfnr, "call_forward_no_reply",      shCallForward,               "^\\*61\\*<number:[0-9]+>#$",                     "^#61#$",                   "^\\*#61#$",0,},
	{suppl_serv_type_cfnr, "call_forward_no_reply2",     shCallForward,               "^\\*61\\*<number:[0-9]+>\\*<timer:[0-9]+>#$", 	"^#61#$",                   "^\\*#61#$",0,},
	{suppl_serv_type_cfnr, "call_forward_no_reply3",     shCallForward,               "",                                               "",                         "",         0,},
	{suppl_serv_type_cfnr, "call_forward_no_reply4",     shCallForward,               "",                                               "",                         "",         0,},
	{suppl_serv_type_cfntr,"call_forward_not_reachable", shCallForward,               "^\\*62\\*<number:[0-9]+>#$",                     "^#62#$",                   "^\\*#62#$",0,},

	{suppl_serv_type_clip, "clip",                       shClip,                      "^\\*30\\#$",                                     "^#30#$",                   "^\\*#30#$",0,},
	{suppl_serv_type_clip2,"clip_per_call",              shClipClirPerCall,           "",                                               "",                         "",         0,},
	{suppl_serv_type_clir, "clir",                       shClir,                      "^\\*31\\#$",                                     "^#31#$",                   "^\\*#31#$",0,},
	{suppl_serv_type_clir2,"clir_per_call",              shClipClirPerCall,           "",                                               "",                         "",         0,},

	{suppl_serv_type_cw,   "call_waiting",               shCallWaiting,               "^\\*43\\#$",                                     "^#43#$",                   "^\\*#43#$",0,},
	{suppl_serv_type_cbi,  "call_barring_international", shCallBarringInternational,  "^\\*331\\*<pin:[0-9]+>#$",                       "^\\#331\\*<pin:[0-9]+>#$", "^\\*#331#$",0,},

	{suppl_serv_type_none, 0,}
};


static const char* ccfc_reason_names[] = { "unconditional", "busy", "no_reply", "not_reachable" };
static const char* at_cmd_fac_outgoing_international_calls = "OI";


////////////////////////////////////////////////////////////////////////////////
// AT command functions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
static int handle_clck_query(const char* facility)
{
	int ok;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	char value[ sizeof("enabled xxx")];    // TODO: arbitrary, make it better
	char* command=alloca(STRLEN("AT+CLCK=") + (strlen(facility) + 3) + 2 + 6 + 1 );
	sprintf(command, "AT+CLCK=\"%s\",2,\"\"", facility);
	char* r = response;
	char* v;

	rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "command.result_name"), "call_barring.OI");

	if (at_send(command, response, "+CLCK", &ok, 0) != 0 || !ok)
	{
		return -1;
	}
	if (*(response + STRLEN("+CLCK: ")) == '0')
	{
		return rdb_set_single(rdb_name(RDB_UMTS_SERVICES".call_barring", facility), "disabled");
	}
	strcpy(value, "enabled ");
	v = value + STRLEN("enabled ");
	// parse something like: +CLCK: 1,1
	while (*r)
	{
		r += STRLEN("+CLCK: 1,");
		for (; *r != '\0' && *r != '\n'; ++r)
		{
			*v++ = *r;
		}
		if (*r == '\n')
		{
			++r;
			*v++ = ',';
		}
	}
	*v = 0;
	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES".call_barring", facility), value);
}
////////////////////////////////////////////////////////////////////////////////
int handle_clck(const char* facility, at_cmd_enable_disable_enum mode, const char* pin, int pin_size, at_cmd_class_enum classx)
{
	int ok;
	char* command=alloca( STRLEN("AT+CLCK=") + (strlen(facility) + 3) + 2 + (pin_size + 3) + 3 + 1 );
	char* pin_string=alloca( pin_size + 1 );
	memcpy(pin_string, pin, pin_size);
	pin_string[ pin_size ] = 0;
	sprintf(command, "AT+CLCK=\"%s\",%d,\"%s\",%d", facility, mode, pin_string, classx);
	if (at_send(command, 0, "", &ok, 0) != 0 || !ok)
	{
		return -1;

	}
	handle_clck_query(facility);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int handle_clir_query(int* m)
{
	int ok;
	char response[ sizeof("+CLIR: 0,0")];
	
	if(isNetworkTestMode())
		return 0;
	
	if (at_send_with_timeout("AT+CLIR?", response, "+CLIR", &ok, TIMEOUT_NETWORK_QUERY, sizeof("+CLIR: 0,0")) != 0 || !ok)
	{
		return -1;
	}
	
	rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "command.result_name"), "clir.status");

	char* str_m;
	int int_m;
	
	char* str_n;
	int int_n;
	
	int clir;
	
	// get N
	if(strlen(response)<STRLEN("+CLIR: ")) {
		int_n=-1;
	}
	else {
		str_n=response + STRLEN("+CLIR: ");
		int_n=atoi(str_n);
	}
	
	// get M
	if(strlen(response)<STRLEN("+CLIR: x,")) {
		int_m=-1;
	}
	else {
		str_m=response + STRLEN("+CLIR: x,");
		int_m=atoi(str_m);
	}
	
	if(m)
		*m=int_m;
	
/*
	<n>: integer type (parameter sets the adjustment for outgoing calls)
	0 presentation indicator is used according to the subscription of the CLIR service
	1 CLIR invocation
	2 CLIR suppression
	
	<m>: integer type (parameter shows the subscriber CLIR service status in the network)
	0 CLIR not provisioned
	1 CLIR provisioned in permanent mode
	2 unknown (e.g. no network, etc.)
	3 CLIR temporary mode presentation restricted
	4 CLIR temporary mode presentation allowed
*/		
	clir=0;
	
	switch(int_n) {
		case 0:
			switch(int_m) {
				case 0:
					clir=0;
					break;
					
				case 1:
					clir=1;
					break;
					
				case 3:
					clir=1;
					break;
					
				case 4:
					clir=0;
					break;
			}
			break;
			
		case 1:
			clir=1;
			break;
			
		case 2:
			clir=0;
			break;
	}
	
	//return rdb_set_single( rdb_name( RDB_UMTS_SERVICES, "clir.status" ), response + STRLEN( "+CLIR: " ) ); // quick and dirty
	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "clir.status"), clir ? "enabled" : "disabled");         // quick and dirty
}

////////////////////////////////////////////////////////////////////////////////
static int handle_clir(at_cmd_enable_disable_enum mode)
{
	int ok;
	
	int m;
	
	if(isNetworkTestMode())
		return 0;
	
	if(handle_clir_query(&m)<0)
		return -1;
	
	if( (m==3) || (m==4) ) {
		if (at_send(mode == at_cmd_enable ? "AT+CLIR=1" : "AT+CLIR=2", 0, "", &ok, 0) != 0 || !ok)
			return -1;
		handle_clir_query(0);
	}
	
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
static int handle_clip_query(void)
{
	int ok;
	char response[ sizeof("+CLIP: 0,0")];
	if (at_send_with_timeout("AT+CLIP?", response, "+CLIP", &ok, TIMEOUT_NETWORK_QUERY, sizeof("+CLIP: 0,0")) != 0 || !ok)
	{
		return -1;
	}

	rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "command.result_name"), "clip.status");

	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "clip.status"), *(response + STRLEN("+CLIP: ")) == '0' ? "disabled" : "enabled");         // quick and dirty
}
////////////////////////////////////////////////////////////////////////////////
int handle_clip(at_cmd_enable_disable_enum n)
{
	int ok;
	if (at_send_with_timeout(n == at_cmd_enable ? "AT+CLIP=1" : "AT+CLIP=0", 0, "", &ok, TIMEOUT_NETWORK_QUERY, 0) != 0 || !ok)
	{
		return -1;
	}
	handle_clip_query();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int handle_ccwa_query(void)
{
	int ok;
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	char* r;
	char* end;
	char class_str[4];
	char* w;

	rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "command.result_name"), "call_waiting.status");

	if (at_send_with_timeout("AT+CCWA=1,2", response, "+CCWA", &ok, TIMEOUT_NETWORK_QUERY, 0) != 0 || !ok)
	{
		return -1;
	}
	if (*(response + STRLEN("+CCWA: ")) == '0')
	{
		return rdb_set_single(rdb_name(RDB_UMTS_SERVICES".call_waiting.status", ""), "disabled");
	}
	/* some sim card returns multi line response even without disable class response line as below;
	 * ex) when voice call waiting is disabled and send query command +ccfc=1,2,1
	 * 		+CCWA: 1,4
	 * 		+CCWA: 1,16
	 *		+CCWA: 1,32
	 */
	for (r = response, end = response + strlen(response); r < end;)
	{
		r += STRLEN("+CCWA: 1,");
		w = &class_str[0];
		(void) memset(class_str, 0x00, 4);
		for (; *r && *r != '\n'; r++)
		{
			*w++ = *r;
		}
		SYSLOG_DEBUG("found ccfc enabled class %s", class_str);
		if (atoi(class_str) == 1)
			return rdb_set_single(rdb_name(RDB_UMTS_SERVICES".call_waiting.status", ""), "enabled");
		r++;
	}
	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES".call_waiting.status", ""), "disabled");
}
////////////////////////////////////////////////////////////////////////////////
int handle_ccfc_query(ccfc_reason_enum reason)
{
	int ok;
	char command[ STRLEN("AT+CCFC=") + 2 + 2 + 1 ];
	char response[AT_RESPONSE_MAX_SIZE]; // TODO: arbitrary, make it better
	char value[256]; // TODO: arbitrary, make it better
	size_t prefix_size = STRLEN("+CCFC: ");
	char* r = response;
	char* v;
	int got_comma;
	sprintf(command, "AT+CCFC=%d,2", reason);

	switch(reason)
	{
		case ccfc_unconditional:
			rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "command.result_name"), "call_forwarding.unconditional");
			break;

		case ccfc_busy:
			rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "command.result_name"), "call_forwarding.busy");
			break;

		case ccfc_no_reply:
			rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "command.result_name"), "call_forwarding.no_reply");
			break;

		case ccfc_not_reachable:
			rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "command.result_name"), "call_forwarding.not_reachable");
			break;

		default:
			rdb_set_single(rdb_name(RDB_UMTS_SERVICES, "command.result_name"), "");
			break;
	}

	if (at_send_with_timeout(command, response, "+CCFC", &ok, TIMEOUT_NETWORK_QUERY, 0) != 0 || !ok)
	{
		return -1;
	}
	if (*(response + prefix_size) == '0')
	{
		return rdb_set_single(rdb_name(RDB_UMTS_SERVICES".call_forwarding", ccfc_reason_names[reason]), "disabled");
	}
	strcpy(value, "enabled ");
	v = value + STRLEN("enabled ");
	// parse something like: +CCFC: 1,1,"+61424610000",145,,,
	while (*r)
	{
		r += (prefix_size + 2);
		for (got_comma = 0; *r != '\0' && *r != '\n' && (*r != ',' || !got_comma); ++r)
		{
			*v++ = *r;
			if (*r == ',')
			{
				got_comma = 1;
			}
		}
		if (*r == ',')
		{
			for (; *r != '\0' && *r != '\n'; ++r);
		}
		if (*r == '\n')
		{
			++r;
			*v++ = ',';
		}
	}
	*v = 0;
	return rdb_set_single(rdb_name(RDB_UMTS_SERVICES".call_forwarding", ccfc_reason_names[reason]), value);
}
////////////////////////////////////////////////////////////////////////////////
static int handle_ccwa(at_cmd_enable_disable_enum n, at_cmd_enable_disable_enum mode, at_cmd_class_enum classx)
{
	int ok;
	char* command=alloca( strlen("AT+CCWA=") + 2 + 2 + 3 + 1 );
	sprintf(command, "AT+CCWA=%d,%d,%d", n, mode, classx);
	if (at_send_with_timeout(command, 0, "", &ok, TIMEOUT_NETWORK_QUERY, 0) != 0 || !ok)
	{
		return -1;
	}
	handle_ccwa_query();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
static int handle_ccfc_disable(ccfc_reason_enum reason, at_cmd_class_enum classx)
{
	int ok;
	char* command=alloca( strlen("AT+CCFC=") + 2 + 2 + 1 + 1 + 3 + 1 );
	/* to prevent "network reject error", changed order as like Disable --> Erase.
	 * In fact, just one of (disable, erase) can clear call forwarding function.
	 */
	sprintf(command, "AT+CCFC=%d,0,,,%d", reason, classx);
	if (at_send_with_timeout(command, 0, "", &ok, TIMEOUT_NETWORK_QUERY, 0) != 0 || !ok)
	{
		return -1;
	}
	sprintf(command, "AT+CCFC=%d,4,,,%d", reason, classx);
	//if( at_send( command, 0, "", &ok, 0 ) != 0 || !ok ) { return -1; }
	at_send_with_timeout(command, 0, "", 0, TIMEOUT_NETWORK_QUERY, 0);   // don't care about the result, as the de-registered number already is not enabled
	handle_ccfc_query(reason);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
static int handle_ccfc_enable(ccfc_reason_enum reason, const char* number, size_t size, at_cmd_class_enum classx, int timeout)
{
	int ok;
	char* command=alloca(strlen("AT+CCFC=") + 2 + 2 + (size + 3) + 4 + 4 + 1);
	char* resp=alloca(1024);
	char* number_string=alloca( size + 1 );
	char timeout_string[3];
	memcpy(number_string, number, size);
	number_string[size] = 0;
	if (timeout < 5 || timeout > 30)
	{
		timeout_string[0] = 0;
	}
	else
	{
		sprintf(timeout_string, "%d", timeout);
	}
	sprintf(command, "AT+CCFC=%d,3,\"%s\",%d,%d,,,%s", reason, number_string, number_string[0] == '+' ? ccfc_international : ccfc_local, classx, timeout_string);
	if (at_send_with_timeout(command, resp, "", &ok, TIMEOUT_NETWORK_QUERY, 0) != 0 || !ok)
		SYSLOG_ERR("ccfc registration failure - %s", resp);

	sprintf(command, "AT+CCFC=%d,1,,,%d,", reason, classx);   // enable for proper class?
	//sprintf( command, "AT+CCFC=%d,1", reason ); // enable for proper class?
	if (at_send_with_timeout(command, resp, "", &ok, TIMEOUT_NETWORK_QUERY, 0) != 0 || !ok)
		SYSLOG_ERR("ccfc enabling failure - %s", resp);

	handle_ccfc_query(reason);
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
static int shCallForward(enum service_action_type actionType,struct supplementary_function_argument* pArg)
{
	struct suppl_serv* pServ=pArg->pServ;
	ccfc_reason_enum ccfcAct;
	int i;
    at_cmd_class_enum class_ar[8] = {at_cmd_class_all, at_cmd_class_pad-1, at_cmd_class_dpa-1, at_cmd_class_dca-1,
                                    at_cmd_class_dcs-1, at_cmd_class_sms-1, at_cmd_class_fax-1, at_cmd_class_data-1};

	// get ccf action type
	switch(pServ->servType)
	{
		case suppl_serv_type_cfi:
			ccfcAct=ccfc_unconditional;
			break;

		case suppl_serv_type_cfb:
			ccfcAct=ccfc_busy;
			break;

		case suppl_serv_type_cfnr:
			ccfcAct=ccfc_no_reply;
			break;

		case suppl_serv_type_cfntr:
			ccfcAct=ccfc_not_reachable;
			break;

		default:
			ccfcAct=ccfc_none;
			break;
	}

	// get command class
	at_cmd_class_enum cmdCls=at_cmd_class_voice;
	switch(pArg->nBS)
	{
		case 1:
			cmdCls=at_cmd_class_voice;
			break;

		case 2:
			cmdCls=at_cmd_class_sms;
			break;

		case 6:
			cmdCls=at_cmd_class_fax;
			break;

		case 7:
			cmdCls=at_cmd_class_dca;
			break;

		case 8:
			cmdCls=at_cmd_class_dcs;
			break;

		case 12:
			//
			// 3gpp document says Voice Group Call Service & Voice Broadcast Service
			// but we don't have any AT command that has such a bloody class. I assume it is voice
			//

			cmdCls=at_cmd_class_voice;
			break;

		case 3:
		case 4:
		case 5:
		default:
			// default voice set
			// without class value, ccfc command does not work well.
			cmdCls=at_cmd_class_voice;
			break;
	}

	// bypass if wrong service type
	if(ccfcAct==ccfc_none)
	{
		fprintf(stderr,"incorrect service type - %d",pServ->servType);
		return -1;
	}

/* If call waiting and call forwarding on busy enabled at the same time,
 * call waiting has higher priority for the second incoming call.
 * If there is a third incoming call, it will be diverted to specified
 * number as a call forwarding on busy scheme.
 */
#ifdef NO_NECESSARY
	else if(ccfcAct==ccfc_busy)
	{
		int stat;
		SYSLOG_INFO("disable call-waiting for call-forwarding-on-busy");
		stat=handle_ccwa(at_cmd_disable, at_cmd_disable, at_cmd_class_all);
		if(stat<0)
			SYSLOG_ERR("failed to disable call-waiting");
	}
#endif

	// call functions
	switch(actionType)
	{
		case service_action_query:
		{
			handle_ccfc_query(ccfcAct);
			break;
		}

		case service_action_deactivate:
		{
		    for (i = 0; i < 8; i++) {
        		SYSLOG_INFO("CCFC disable with class %d", (int)class_ar[i]);
    			if (handle_ccfc_disable(ccfcAct, class_ar[i]) < 0) {
        			SYSLOG_ERR("CCFC disable with class %d failed", (int)class_ar[i]);
    			} else {
    			    break;
    			}
			}
			break;
		}

		case service_action_activate:
		{
			handle_ccfc_enable(ccfcAct, pArg->szNumber, strlen(pArg->szNumber), cmdCls, pArg->nTimer);
			break;
		}
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
static int shClip(enum service_action_type actionType,struct supplementary_function_argument* pArg)
{
	int stat=-1;

	switch(actionType)
	{
		case service_action_query:
			stat=handle_clip_query();
			break;

		case service_action_deactivate:
			stat=handle_clip(at_cmd_disable);
			break;

		case service_action_activate:
			stat=handle_clip(at_cmd_enable);
			break;
	}

	return stat;
}
////////////////////////////////////////////////////////////////////////////////
static int shClipClirPerCallDial(const char* szDN,const char* szPrefix)
{
	int ok;
	char response[AT_RESPONSE_MAX_SIZE];
	char achDial[AT_RESPONSE_MAX_SIZE];

	strcpy(achDial,"ATD");

	if(!strncmp(szDN,"011",3))
	{
		strcat(achDial,"+");
		szDN+=3;
	}
	else if(!strncmp(szDN,"01",2))
	{
		strcat(achDial,"+");
		szDN+=2;
	}

	strcat(achDial,szDN);
	strcat(achDial,szPrefix);
	strcat(achDial,";");

	if ( at_send(achDial,response,"",&ok, 0) != 0 || !ok )
	{
		SYSLOG_ERR("dial command failure - %s", achDial);
		return -1;
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
static int shClipClirPerCall(enum service_action_type actionType,struct supplementary_function_argument* pArg)
{
	int stat;

	if(!pArg->szNumber || !strlen(pArg->szNumber))
		return -1;

	switch(actionType)
	{
		case service_action_deactivate:
			stat=shClipClirPerCallDial(pArg->szNumber,"I");
			break;

		case service_action_activate:
			stat=shClipClirPerCallDial(pArg->szNumber,"i");
			break;

		default:
			stat=-1;
			break;
	}

	return stat;
}
////////////////////////////////////////////////////////////////////////////////
static int shClir(enum service_action_type actionType,struct supplementary_function_argument* pArg)
{
	int stat=-1;

	switch(actionType)
	{
		case service_action_query:
			stat=handle_clir_query(0);
			break;

		case service_action_deactivate:
			stat=handle_clir(at_cmd_disable);
			break;

		case service_action_activate:
			stat=handle_clir(at_cmd_enable);
			break;
	}

	return stat;
}
////////////////////////////////////////////////////////////////////////////////
static int shCallWaiting(enum service_action_type actionType,struct supplementary_function_argument* pArg)
{
	int stat=-1;

	switch(actionType)
	{
		case service_action_query:
			stat=handle_ccwa_query();
			break;

		case service_action_deactivate:
			stat=handle_ccwa(at_cmd_enable, at_cmd_disable, at_cmd_class_voice);
			break;

		case service_action_activate:
			stat=handle_ccwa(at_cmd_enable, at_cmd_enable, at_cmd_class_voice);
			break;
	}

	return stat;
}
////////////////////////////////////////////////////////////////////////////////
static int shCallBarringInternational(enum service_action_type actionType,struct supplementary_function_argument* pArg)
{
	at_cmd_enable_disable_enum enableCmd;

	int stat=-1;

	switch(actionType)
	{
		case service_action_query:
			stat=handle_clck_query(at_cmd_fac_outgoing_international_calls);
			break;

		case service_action_deactivate:
		case service_action_activate:
			enableCmd=(service_action_activate==actionType) ? at_cmd_enable : at_cmd_disable;
			stat=handle_clck(at_cmd_fac_outgoing_international_calls, enableCmd, pArg->szPin, strlen(pArg->szPin), at_cmd_class_voice);
			break;
	}

	return stat;
}

////////////////////////////////////////////////////////////////////////////////
void supplFini(void)
{
	struct suppl_serv* pServ;

	pServ=_supplServ;
	while(pServ->servType!=suppl_serv_type_none)
	{
		if(pServ->pRuleActivate)
			dynaFree(pServ->pRuleActivate);
		pServ->pRuleActivate=0;

		if(pServ->pRuleDeActivate)
			dynaFree(pServ->pRuleDeActivate);
		pServ->pRuleDeActivate=0;

		if(pServ->pRuleQuery)
			dynaFree(pServ->pRuleQuery);
		pServ->pRuleQuery=0;

		pServ++;
	}
}
////////////////////////////////////////////////////////////////////////////////
int supplInit(void)
{
	struct suppl_serv* pServ;

	// create tag rule objects
	pServ=_supplServ;
	while(pServ->servType!=suppl_serv_type_none)
	{
		pServ->pRuleActivate=tagruleCreate();
		pServ->pRuleDeActivate=tagruleCreate();
		pServ->pRuleQuery=tagruleCreate();

		pServ->fEnabled=1;

		pServ++;
	}

	// set tag rules
	pServ=_supplServ;
	while(pServ->servType!=suppl_serv_type_none)
	{
		tagruleSetRules(pServ->pRuleActivate,pServ->szActivate);
		tagruleSetRules(pServ->pRuleDeActivate,pServ->szDeactivate);
		tagruleSetRules(pServ->pRuleQuery,pServ->szQuery);

		pServ++;
	}

	char* pDbVar=alloca(1024);
	char* pDbVal=alloca(1024);
	int cbDbVal=1024;

	// read rules from database
	pServ=_supplServ;
	while(pServ->servType!=suppl_serv_type_none)
	{

		// set activate
		sprintf(pDbVar,"%s.%s.%s",DB_SUPPL_PREFIX,pServ->szServ,DB_SUFIX_ACT);
		if(rdb_get_single(pDbVar,pDbVal,cbDbVal)>=0)
			tagruleSetRules(pServ->pRuleActivate,pDbVal);

		// set deactivate
		sprintf(pDbVar,"%s.%s.%s",DB_SUPPL_PREFIX,pServ->szServ,DB_SUFIX_DEACT);
		if(rdb_get_single(pDbVar,pDbVal,cbDbVal)>=0)
			tagruleSetRules(pServ->pRuleDeActivate,pDbVal);

		// set query
		sprintf(pDbVar,"%s.%s.%s",DB_SUPPL_PREFIX,pServ->szServ,DB_SUFIX_QUERY);
		if(rdb_get_single(pDbVar,pDbVal,cbDbVal)>=0)
			tagruleSetRules(pServ->pRuleQuery,pDbVal);

		pServ++;
	}


	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int handleSupplServ(const struct name_value_t* args)
{
	int stat=-1;

	struct suppl_serv* pServ;
	struct supplementary_function_argument supplArg;

	const char* szStr=args[0].value;

	char* pNumber=alloca(SUPPLE_MAX_NUMBER);
	char* pTimer=alloca(SUPPLE_MAX_NUMBER);
	char* pBS=alloca(SUPPLE_MAX_NUMBER);
	char* pPin=alloca(SUPPLE_MAX_NUMBER);

	pServ=_supplServ;
	while(pServ->servType!=suppl_serv_type_none)
	{
		memset(&supplArg,0,sizeof(supplArg));
		supplArg.pServ=pServ;

		suppl_func lpfnHandler=(suppl_func)(pServ->lpfnHandler);

		// activate
		if( tagruleIsMatched(pServ->pRuleActivate,szStr) )
		{
			struct tagrule* pT=pServ->pRuleActivate;

			// get timer
			pTimer[0]=0;
			tagruleGetMatchbyTag(pT,szStr,"timer", pTimer,SUPPLE_MAX_NUMBER);
			supplArg.nTimer=atoi(pTimer);

			// get bs
			tagruleGetMatchbyTag(pT,szStr,"bs", pBS,SUPPLE_MAX_NUMBER);
			supplArg.nBS=atoi(pBS);

			supplArg.szNumber=tagruleGetMatchbyTag(pT,szStr,"number", pNumber,SUPPLE_MAX_NUMBER);
			supplArg.szPin=tagruleGetMatchbyTag(pT,szStr,"pin", pPin,SUPPLE_MAX_NUMBER);

			lpfnHandler(service_action_activate,&supplArg);

			stat=0;

			break;
		}
		// deactivate
		else if( tagruleIsMatched(pServ->pRuleDeActivate,szStr) )
		{
			struct tagrule* pT=pServ->pRuleDeActivate;

			supplArg.szNumber=tagruleGetMatchbyTag(pT,szStr,"number", pNumber,SUPPLE_MAX_NUMBER);
			supplArg.szPin=tagruleGetMatchbyTag(pT,szStr,"pin", pPin,SUPPLE_MAX_NUMBER);

			lpfnHandler(service_action_deactivate,&supplArg);

			stat=0;

			break;
		}
		// query
		else if ( tagruleIsMatched(pServ->pRuleQuery,szStr) )
		{
			lpfnHandler(service_action_query,&supplArg);

			stat=0;

			break;
		}

		pServ++;
	}

	return stat;
}

////////////////////////////////////////////////////////////////////////////////

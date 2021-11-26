/* ----------------------------------------------------------------------------
RDB interface program

Lee Huang<leeh@netcomm.com.au>

*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "rdb_comms.h"
#include "utils.h"

extern char rdb_prefix[40];
struct rdb_session *g_rdb_session;

const TRdbNameList g_rdbNameList[] =
{

	{Diagnostics_UDPEcho_DiagnosticsState, 1, 0, "None"},
	{Diagnostics_UDPEcho_Changed, 1, RDB_VAR_SUBCRIBE, ""},
	{Diagnostics_UDPEcho_StartTest, 1, 0, "0"},
	{Diagnostics_UDPEcho_SmartEdgeAddress, 1, RDB_VAR_SET_IF, "0.0.0.0"},
	{Diagnostics_UDPEcho_MPLSTag, 1, RDB_VAR_SET_IF, "0"},
	{Diagnostics_UDPEcho_CoS, 1, RDB_VAR_SET_IF, "0"},
	{Diagnostics_UDPEcho_CoStoEXP, 1, RDB_VAR_SET_IF, "0,1,2,3,4,5,6,7"},
	{Diagnostics_UDPEcho_CoStoDSCP, 1, RDB_VAR_SET_IF, "0,0,0,16,32,40,0,0"},
	{Diagnostics_UDPEcho_InterfaceAddress, 1, RDB_VAR_SET_IF, "0.0.0.0"},
	{Diagnostics_UDPEcho_InterfaceNetmask, 1, RDB_VAR_SET_IF, "0.0.0.0"},
	{Diagnostics_UDPEcho_ServerAddress, 1, 0, "0.0.0.0"},
	{Diagnostics_UDPEcho_ServerPort, 1, 0, "5000"},
	{Diagnostics_UDPEcho_PacketCount, 1, 0, "0"},
	{Diagnostics_UDPEcho_PacketSize, 1, 0, "20"},
	{Diagnostics_UDPEcho_PacketInterval, 1, 0, 	"1000"},
	{Diagnostics_UDPEcho_StragglerTimeout, 1, 0, "5000"},
	{Diagnostics_UDPEcho_BytesSent, 1, 0, "0"},
	{Diagnostics_UDPEcho_BytesReceived, 1, 0, "0"},
	{Diagnostics_UDPEcho_PacketsSent, 1, 0, "0"},
	{Diagnostics_UDPEcho_PacketsSendLoss, 1, 0, "0"},
	{Diagnostics_UDPEcho_PacketsReceived, 1, 0, "0"},
	{Diagnostics_UDPEcho_PacketsReceiveLoss, 1, 0, "0"},
	{Diagnostics_UDPEcho_PacketsLossPercentage, 1, 0, "0"},
	{Diagnostics_UDPEcho_RTTAverage, 1, 0, "0"},
	{Diagnostics_UDPEcho_RTTMax, 1, 0, "0"},
	{Diagnostics_UDPEcho_RTTMin, 1, 0, "0"},
	{Diagnostics_UDPEcho_SendPathDelayDeltaJitterAverage, 1, 0, "0"},
	{Diagnostics_UDPEcho_SendPathDelayDeltaJitterMin, 1, 0,  "0"},
	{Diagnostics_UDPEcho_SendPathDelayDeltaJitterMax, 1, 0, "0"},
	{Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterAverage, 1, 0, "0"},
	{Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterMin, 1, 0, "0"},
	{Diagnostics_UDPEcho_ReceivePathDelayDeltaJitterMax, 1, 0, "0"},

	{Diagnostics_UDPEcho_TC1_CIR, 1, RDB_VAR_SET_IF, "165000"},
	{Diagnostics_UDPEcho_TC2_PIR, 1, RDB_VAR_SET_IF, "10000000"},
	{Diagnostics_UDPEcho_TC4_PIR, 1, RDB_VAR_SET_IF, "2200000"},

	{Diagnostics_UDPEcho_RXPkts, 1, 0, "0"},
	{Diagnostics_UDPEcho_TXPkts, 1, 0, "0"},

	{Diagnostics_UDPEcho_RXPktsSmartedge, 1, 0, "0"},
	{Diagnostics_UDPEcho_TXPktsSmartedge, 1, 0, "0"},

#ifdef _DUMP_RAW
	{Diagnostics_UDPEcho_StatsName, 1,0,""},
#endif
    {0,},

};


#if 0
///copy of g_rdbNameList, but the RBD name will included the session name
TRdbNameList g_rdbSessionNameList[ sizeof(g_rdbNameList)/ sizeof(TRdbNameList)];



const char *get_rdb_name(enum TRdbNameIndex index)
{
	return g_rdbSessionNameList[index].szName;
}


int build_rdb_session_variables( int session_id)
{
	const TRdbNameList* pNameList= g_rdbNameList;
	char buf[20];
	sprintf(buf, ".%d.", session_id);

	TRdbNameList* pSessionNameList= g_rdbSessionNameList;

    while(pNameList->szName)
    {
        char s[128];

        memcpy(pSessionNameList, pNameList, sizeof(TRdbNameList));
		// if it is not persistent RDB
		if((pNameList->attribute &RDB_STATIC) ==0 && session_id >0)
        {
        	// goto the second '.'
			const char *p = strchr(pNameList->szName, '.');
			if(p) p = strchr(p+1, '.');
			if(p)
			{
				int len = (int)(p - pNameList->szName);

				memcpy(s, pNameList->szName, len);
				strcpy(&s[len], buf);
				strcat(&s[len], p+1);
				pSessionNameList->szName = strdup(s);
			}
			else
			{
				goto lab_error;
			}
        }
		else
		{
			pSessionNameList->szName = strdup(pNameList->szName);

		}
		if(pSessionNameList->szName ==NULL) goto lab_error;
	    pNameList++;
	    pSessionNameList++;
    }
	return 0;
lab_error:
	rdb_end( 1 );
	return -1;
}

#endif

////////////////////////////////////////////////////////////////////////////////
static int create_rdb_variables(int session_id)
{

    const TRdbNameList* pNameList= g_rdbNameList;

	while(pNameList->szName)
	{
		char name[128];
		char s[128];
		int len=127;
		rdb_get_i_name(name, pNameList->szName, session_id);
		if (rdb_get(g_rdb_session, name, s, &len) != 0)
		{
			NTCLOG_DEBUG("create '%s'", name);
			if(rdb_create(g_rdb_session, name, "", 1, CREATE, ALL_PERM)<0)
			{
				NTCLOG_ERR("failed to create '%s'", name);
			}
			else if(pNameList->szValue)
			{
				rdb_set_string(g_rdb_session, name, pNameList->szValue);
			}
		}
		else if( (pNameList->attribute &(RDB_VAR_SUBCRIBE_TEST|RDB_VAR_SUBCRIBE)) &&
			pNameList->szValue)
		{
			rdb_set_string(g_rdb_session, name, pNameList->szValue);
		}

		pNameList++;
	}
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// initilize rdb variable
// session_id (in) -- <=0 no session  id in the rdb variable
// fCreate (in) -- force create all the variable
int rdb_init(int session_id, int fCreate)
{
    if ((rdb_open(RDB_DEVNAME, &g_rdb_session)) < 0)
    {
        NTCLOG_ERR("failed to open RDB!");
        return -1;
    }


	if(fCreate)
	{
		create_rdb_variables(session_id);
	}


    rdb_close(&g_rdb_session);

	g_rdb_session =NULL;

    return 0;
}

void rdb_end(int session_id, int remove_rdb )
{

	const TRdbNameList* pNameList= g_rdbNameList;
	rdb_lock(g_rdb_session, 1);
	if(remove_rdb)
	{
		while(pNameList->szName)
		{
			if(pNameList->bcreate )
			{
				char name[128];
				rdb_get_i_name(name, pNameList->szName, session_id);
				rdb_delete(g_rdb_session, name);

			}

			pNameList++;
		}
	}
	rdb_unlock(g_rdb_session);
}


////////////////////////////////////////////////////////////////////////////////
int subscribe(int session_id, int subs_group)
{
   const TRdbNameList* pNameList= g_rdbNameList;

    while(pNameList->szName)
    {
        if( (pNameList->attribute & subs_group) )
        {

			char name[128];
			rdb_get_i_name(name, pNameList->szName, session_id);

            NTCLOG_DEBUG("subscribing %s",name);
            if (rdb_subscribe(g_rdb_session, name) <  0)
            {
                NTCLOG_ERR("failed to subscribe to '%s'!", name);
            }
        }

        pNameList++;
    }

    return 0;
}


////////////////////////
int rdb_get_boolean(const char* name, int *value)
{
    char buf[12];
    int len=12;
    int retValue = rdb_get(g_rdb_session, name, buf, &len);
    if(retValue)
    {
        return retValue;
    }
    *value = buf[0] !='0' && buf[0] !=0;
    return 0;
}

////////////////////////////////////////
int rdb_set_boolean(const char* name, int value)
{
    char buf[2];
    int retValue;
    buf[0] =value !=0?'1':'0';
    buf[1] =0;
    retValue = rdb_set_str(name, buf);
    if(retValue)
    {
        return retValue;
    }
    return 0;
}
//////////////////////////////////////////
int rdb_get_uint(const char* name, unsigned int *value)
{
    char buf[32];
    int retValue;
    int len=31;
    retValue = rdb_get(g_rdb_session, name, buf, &len);
    if(retValue)
    {
        return retValue;
    }
    buf[31]=0;
    *value = Atoi(buf, &retValue);
    if(!retValue)
    {
        return -1;
    }
    return 0;
}
//////////////////////////////////////////////
int rdb_set_uint(const char* name, unsigned int value)
{
    char buf[32];
    int retValue;
    sprintf(buf, "%u", value);
    retValue = rdb_set_str(name, buf);
    if(retValue)
    {
        return retValue;
    }
    return 0;
}

int rdb_set_int(const char* name, int value)
{
    char buf[32];
    int retValue;
    sprintf(buf, "%d", value);
    retValue = rdb_set_str(name, buf);
    if(retValue)
    {
        return retValue;
    }
    return 0;
}

int rdb_set_float(const char*fmt, const char* name, double value)
{
    char buf[32];
    int retValue;
    sprintf(buf, fmt, value);
    retValue = rdb_set_str(name, buf);
    if(retValue)
    {
        return retValue;
    }
    return 0;

}


char* rdb_get_i_name(char *buf, const char *namefmt, int i)
{
	int len;
	strcpy(buf, rdb_prefix);
	len =strlen(buf);
	if(i ==0)
	{
		// remove %i. in name fmt
		const char *p1 =namefmt;
		char *p2 = &buf[len];
		while(*p1)
		{
			if(*p1 =='%') // skip "%i."
			{
				p1 +=3;
			}
			*p2 ++=*p1 ++;
		}
		*p2 =0;

	}
	else
	{
		sprintf(&buf[len], namefmt, i);
	}
	return buf;
}

int rdb_get_i_uint(const char* namefmt, int i, unsigned int *value)
{
	char name[128];
	rdb_get_i_name(name, namefmt, i);
	return rdb_get_uint(name, value);
}
//////////////////////////////////////////////
int rdb_set_i_uint(const char* namefmt, int i, unsigned int value)
{
	char name[128];
	rdb_get_i_name(name, namefmt, i);
	return rdb_set_uint(name, value);
}

int rdb_get_i_str(const char* namefmt, int i, char* value, int len)
{
	char name[128];
	char temp[128];
	/*it is no safe to pass NULL parameter to rdb_get in older RDB driver*/
	if(value == NULL){
		value = temp;
		len = 0;
	}
	rdb_get_i_name(name, namefmt, i);
	return rdb_get(g_rdb_session, name, value, &len);

}

int rdb_set_i_str(const char* namefmt, int i, const char* value)
{
	char name[128];
	rdb_get_i_name(name, namefmt, i);
	return rdb_set_string(g_rdb_session, name, value);

}

int rdb_set_i_float(const char*fmt, const char* namefmt, int i,  double value)
{
	char name[128];
	rdb_get_i_name(name, namefmt, i);
	return rdb_set_float(fmt, name, value);

}


int rdb_get_i_boolean(const char* namefmt, int i,  int *value)
{
	char name[128];
	rdb_get_i_name(name, namefmt, i);
	return rdb_get_boolean(name, value);
}
//////////////////////////////////////////////
int rdb_set_i_boolean(const char* namefmt, int i, int value)
{
	char name[128];
	rdb_get_i_name(name, namefmt, i);
	return rdb_set_boolean(name, value);
}

// poll any rdb changed
// $ >0   --- rdb changed
// $ 0 --- rdb not changed
// $ <0 -- rdb error
int poll_rdb(int timeout_sec, int timeout_usec)
{
/* initialize loop variables */
	fd_set fdr;
	int selected;
	int fd = rdb_fd(g_rdb_session);
	struct timeval tv = { .tv_sec = timeout_sec, .tv_usec =timeout_usec};

	FD_ZERO(&fdr);

	/* put database into fd read set */
	FD_SET(fd, &fdr);

	/* select */
	selected = select(fd+1 , &fdr, NULL, NULL, &tv);
	if (selected > 0  )
	{
		if(FD_ISSET(fd, &fdr))	return selected;// detect fd
		// no detected
		return 0;
	}
	// could be 0, or -1
	return selected;
}


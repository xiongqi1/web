/* ----------------------------------------------------------------------------
RDB interface program

Lee Huang<leeh@netcomm.com.au>

*/
#include <stdio.h>
#include "log.h"
#include "rdb_comms.h"
#include "utils.h"


const TRdbNameList g_rdbNameList[] =
{

	{UDPEchoConfig, 0, RDB_VAR_PREFIX,},
	{UDPEchoConfig_Enable, 1, RDB_VAR_SUBCRIBE, "0"},
	{UDPEchoConfig_Interface, 1, RDB_VAR_SET_IF, ""},
	{UDPEchoConfig_SourceIPAddress, 1, RDB_VAR_SET_IF, "0.0.0.0"},
	{UDPEchoConfig_UDPPort, 1, RDB_VAR_SET_IF, "8088"},
	{UDPEchoConfig_EchoPlusEnabled, 1, RDB_VAR_SUBCRIBE, "1"},
	{UDPEchoConfig_PacketsReceived, 1, RDB_VAR_SET_IF, ""},
	{UDPEchoConfig_PacketsResponded, 1, RDB_VAR_SET_IF, ""},
	{UDPEchoConfig_BytesReceived, 1, RDB_VAR_SET_IF, ""},
	{UDPEchoConfig_BytesResponded, 1, RDB_VAR_SET_IF, ""},
	{UDPEchoConfig_TimeFirstPacketReceived, 1, 0, ""},
	{UDPEchoConfig_TimeLastPacketReceived, 1, 0, ""},

   {0,},

};

////////////////////////////////////////////////////////////////////////////////
static int create_rdb_variables(int fCreate)
{

    const TRdbNameList* pNameList= g_rdbNameList;

    while(pNameList->szName)
    {
        char s[64];
        if(fCreate && pNameList->bcreate)
        {
            if (rdb_get_single(pNameList->szName, s, 64) != 0)
            {
                SYSLOG_DEBUG("create '%s'", pNameList->szName);
                if(rdb_create_variable(pNameList->szName, "", CREATE, ALL_PERM, 0, 0)<0)
                {
                    SYSLOG_DEBUG("failed to create '%s'", pNameList->szName);
                }
                else if(pNameList->szValue)
                {
                	rdb_set_single(pNameList->szName, pNameList->szValue);
                }
            }
        }

        pNameList++;
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// initilize rdb variable
// fCreate (in) -- force create all the variable
int rdb_init(int fCreate)
{
    if ((rdb_open_db()) < 0)
    {
        SYSLOG_DEBUG("failed to open RDB!");
        return -1;
    }

    create_rdb_variables(fCreate);



    rdb_close_db();


    return 0;
}
////////////////////////////////////////////////////////////////////////////////
int subscribe(void)
{
    const TRdbNameList* pNameList= g_rdbNameList;

    while(pNameList->szName)
    {
        if(pNameList->attribute & RDB_VAR_SUBCRIBE)
        {
            SYSLOG_DEBUG("subscribing %s",pNameList->szName);
            if (rdb_subscribe_variable(pNameList->szName) <  0)
            {
                SYSLOG_DEBUG("failed to subscribe to '%s'!", pNameList->szName);
            }
        }

        pNameList++;
    }

    //handle_rdb_events();

    return 0;
}


////////////////////////
int rdb_get_boolean(const char* name, int *value)
{
    char buf[12];
    int retValue = rdb_get_single(name, buf,12);
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
int rdb_getint(const char* name, unsigned int *value)
{
    char buf[32];
    int retValue;
    retValue = rdb_get_single(name, buf,31);
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
#include "log.h"
//////////////////////////////////////////////
int rdb_set_int(const char* name, unsigned int value)
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

int rdb_set_sint(const char* name, int value)
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


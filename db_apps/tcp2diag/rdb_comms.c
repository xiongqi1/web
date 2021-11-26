#include <stdio.h>
#include "rdb_ops.h"

#undef DEBUG
// #define DEBUG

#ifdef DEBUG
#define SYSLOG_ERR(...)     printf(__VA_ARGS__); putchar('\n')
#define SYSLOG_INFO(...)    printf(__VA_ARGS__); putchar('\n')
#define SYSLOG_DEBUG(...)   printf(__VA_ARGS__); putchar('\n')
#else
#define SYSLOG_ERR(...)
#define SYSLOG_INFO(...)
#define SYSLOG_DEBUG(...)
#endif

#define RDB_VAR_NONE 		0
#define RDB_VAR_SET_IF 		0x01 // used by IF interface
#define RDB_VAR_PREFIX		0x10 // it is prefix of RDB variable
#define RDB_VAR_SUBCRIBE	0x80 // subscribed

typedef struct TRdbNameList
{
    const char* szName;
    int bcreate; // create if it does not exit
    int attribute;	//RDB_VAR_XXXX
    const char*szValue; // default value
} TRdbNameList;

const TRdbNameList g_rdbNameList[] =
{

	{"wwan.0.diag_if", 1, RDB_VAR_SUBCRIBE, ""},

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
static int subscribe(void)
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
int  open_rdb()
{
    int retValue = rdb_open_db();
    if(retValue <0) return retValue;

    retValue = subscribe();
    if(retValue <0) return retValue;

    return retValue;
}
////////////////////////////////////////////////////////////////////////////////

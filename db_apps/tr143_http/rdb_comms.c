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
	//{Diagnostics_Capabilities_DownloadTransports, 1, RDB_GROUP_DOWNLOAD|RDB_STATIC, "HTTP"},
	{Diagnostics_Download_DiagnosticsState, 1, RDB_GROUP_DOWNLOAD, "None"},
	{Diagnostics_Download_Changed,  1, RDB_GROUP_DOWNLOAD|RDB_VAR_SUBCRIBE, ""},
	{Diagnostics_Download_StartTest,  1, RDB_GROUP_DOWNLOAD, "0"},
	{Diagnostics_Download_SmartEdgeAddress, 1, RDB_GROUP_DOWNLOAD|RDB_VAR_DOWNLOAD_IF, "0.0.0.0"},
	{Diagnostics_Download_MPLSTag, 1, RDB_GROUP_DOWNLOAD|RDB_VAR_DOWNLOAD_IF, "0"},
	{Diagnostics_Download_CoS, 1, RDB_GROUP_DOWNLOAD|RDB_VAR_DOWNLOAD_IF,	"0"},
	{Diagnostics_Download_CoStoEXP, 1, RDB_GROUP_DOWNLOAD|RDB_VAR_DOWNLOAD_IF, "0,1,2,3,4,5,6,7"},
	{Diagnostics_Download_CoStoDSCP, 1, RDB_GROUP_DOWNLOAD|RDB_VAR_DOWNLOAD_IF, "0,0,0,16,32,40,0,0"},
	{Diagnostics_Download_InterfaceAddress, 1, RDB_GROUP_DOWNLOAD|RDB_VAR_DOWNLOAD_IF,	"0.0.0.0"},
	{Diagnostics_Download_InterfaceNetmask, 1, RDB_GROUP_DOWNLOAD|RDB_VAR_DOWNLOAD_IF, "0.0.0.0"},
	{Diagnostics_Download_DownloadURL, 1, RDB_GROUP_DOWNLOAD},
	{Diagnostics_Download_ROMTime, 1, RDB_GROUP_DOWNLOAD},
	{Diagnostics_Download_BOMTime, 1, RDB_GROUP_DOWNLOAD},
	{Diagnostics_Download_EOMTime, 1, RDB_GROUP_DOWNLOAD},
	{Diagnostics_Download_TestBytesReceived, 1, RDB_GROUP_DOWNLOAD, "0"},
	{Diagnostics_Download_TotalBytesReceived, 1, RDB_GROUP_DOWNLOAD, "0"},
	//{Diagnostics_Download_SampleInterval, 1, RDB_GROUP_DOWNLOAD, "5"},
	{Diagnostics_Download_MinThroughput, 1, RDB_GROUP_DOWNLOAD, "0"},
	{Diagnostics_Download_MaxThroughput, 1, RDB_GROUP_DOWNLOAD, "0"},
	{Diagnostics_Download_Throughput, 1, RDB_GROUP_DOWNLOAD, "0"},

	{Diagnostics_Download_TC1_CIR, 1, RDB_GROUP_DOWNLOAD|RDB_VAR_DOWNLOAD_IF, "165000"},
        {Diagnostics_Download_TC2_PIR, 1, RDB_GROUP_DOWNLOAD|RDB_VAR_DOWNLOAD_IF, "10000000"},
	{Diagnostics_Download_TC4_PIR, 1, RDB_GROUP_DOWNLOAD|RDB_VAR_DOWNLOAD_IF, "2200000"},


	//{Diagnostics_Capabilities_UploadTransports, 1, RDB_GROUP_UPLOAD|RDB_STATIC, "HTTP"},
	//{Diagnostics_Upload, 1, 0},
	{Diagnostics_Upload_DiagnosticsState, 1, RDB_GROUP_UPLOAD, "None"},
	{Diagnostics_Upload_Changed,  1, RDB_GROUP_UPLOAD|RDB_VAR_SUBCRIBE, ""},
	{Diagnostics_Upload_StartTest,  1, RDB_GROUP_UPLOAD, "0"},
	{Diagnostics_Upload_SmartEdgeAddress, 1, RDB_GROUP_UPLOAD|RDB_VAR_UPLOAD_IF, "0.0.0.0"},
	{Diagnostics_Upload_MPLSTag, 1, RDB_GROUP_UPLOAD|RDB_VAR_UPLOAD_IF, "0"},
	{Diagnostics_Upload_CoS, 1, RDB_GROUP_UPLOAD|RDB_VAR_UPLOAD_IF,	"0"},
	{Diagnostics_Upload_CoStoEXP, 1, RDB_GROUP_UPLOAD|RDB_VAR_UPLOAD_IF, "0,1,2,3,4,5,6,7"},
	{Diagnostics_Upload_CoStoDSCP, 1, RDB_GROUP_UPLOAD|RDB_VAR_UPLOAD_IF, "0,0,0,16,32,40,0,0"},
	{Diagnostics_Upload_InterfaceAddress, 1, RDB_GROUP_UPLOAD|RDB_VAR_UPLOAD_IF,	"0.0.0.0"},
	{Diagnostics_Upload_InterfaceNetmask, 1, RDB_GROUP_UPLOAD|RDB_VAR_UPLOAD_IF, "0.0.0.0"},
	{Diagnostics_Upload_UploadURL, 1, RDB_GROUP_UPLOAD},
	{Diagnostics_Upload_TestFileLength, 1, RDB_GROUP_UPLOAD, "0"},
	{Diagnostics_Upload_ROMTime, 1, RDB_GROUP_UPLOAD},
	{Diagnostics_Upload_BOMTime, 1, RDB_GROUP_UPLOAD},
	{Diagnostics_Upload_EOMTime, 1, RDB_GROUP_UPLOAD},
	{Diagnostics_Upload_TotalBytesSent, 1, RDB_GROUP_UPLOAD, "0"},

	//{Diagnostics_Upload_SampleInterval, 1, RDB_GROUP_UPLOAD, "5"},
	{Diagnostics_Upload_MinThroughput, 1, RDB_GROUP_UPLOAD, "0"},
	{Diagnostics_Upload_MaxThroughput, 1, RDB_GROUP_UPLOAD, "0"},
	{Diagnostics_Upload_Throughput, 1, RDB_GROUP_UPLOAD, "0"},

	{Diagnostics_Upload_TC1_CIR, 1, RDB_GROUP_UPLOAD|RDB_VAR_UPLOAD_IF, "165000"},
	{Diagnostics_Upload_TC2_PIR, 1, RDB_GROUP_UPLOAD|RDB_VAR_UPLOAD_IF, "10000000"},
	{Diagnostics_Upload_TC4_PIR, 1, RDB_GROUP_UPLOAD|RDB_VAR_UPLOAD_IF, "5500000"},

	{Diagnostics_HttpMovingAverageWindowSize, 1,  RDB_STATIC|RDB_GROUP_DOWNLOAD|RDB_GROUP_UPLOAD, "5"},


    {0,},

};



#if 0
///copy of g_rdbNameList, but the RBD name will included the session name
TRdbNameList g_rdbSessionNameList[ sizeof(g_rdbNameList)/ sizeof(TRdbNameList)];



//const char *get_rdb_name(enum TRdbNameIndex index)
//{
//	return g_rdbSessionNameList[index].szName;
//}



#endif

////////////////////////////////////////////////////////////////////////////////
static int create_rdb_variables(int session_id,  int rdb_group)
{

    const TRdbNameList* pNameList= g_rdbNameList;

	while(pNameList->szName)
	{

		if(pNameList->attribute & rdb_group)
		{
			char name[128];
			char s[128];
			int len=127;
			if(pNameList->attribute&RDB_STATIC)
			{
				strcpy(name, pNameList->szName);
			}
			else
			{
				rdb_get_i_name(name, pNameList->szName, session_id);
			}
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
		}
		pNameList++;
	}
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// initilize rdb variable
// session_id (in) -- <=0 no session  id in the rdb variable
// fCreate (in) -- force create all the variable
int rdb_init(int session_id,  int rdb_group, int fCreate)
{
    if ((rdb_open(RDB_DEVNAME, &g_rdb_session)) < 0)
    {
        NTCLOG_ERR("failed to open RDB!");
        return -1;
    }


	if( fCreate)
	{

		create_rdb_variables(session_id, rdb_group);
	}


    rdb_close(&g_rdb_session);

	g_rdb_session =NULL;

    return 0;
}

void rdb_end(int session_id, int rdb_group, int remove_rdb)
{

	const TRdbNameList* pNameList= g_rdbNameList;
	rdb_lock(g_rdb_session, 1);
	if(remove_rdb)
	{
		while(pNameList->szName)
		{
			if(pNameList->bcreate && (pNameList->attribute & rdb_group))
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
int subscribe(int session_id, int rdb_group, int sub_group)
{

    const TRdbNameList* pNameList= g_rdbNameList;

    while(pNameList->szName)
    {
        if( (pNameList->attribute & sub_group) &&
			pNameList->attribute & rdb_group)
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

    //handle_rdb_events();

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
	char temp[1];
	/*it is no safe to pass NULL parameters to rdb_get in older RDB driver*/
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

//////////////////////////////////////////////
// set timestamp rdb, format s.ms
int rdb_set_i_timestamp(const char* namefmt, int i, struct timeval *tv)
{
	char value[64];
	// current tr069 doesnot accept ms for datetime type
	//sprintf(value, "%ld.%ld", tv->tv_sec, tv->tv_usec/USPEMS);
	sprintf(value, "%ld", tv->tv_sec);
	return rdb_set_i_str(namefmt, i, value);
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

/* ----------------------------------------------------------------------------
RDB interface program

Lee Huang<leeh@netcomm.com.au>

*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "rdb_comms.h"
#include "utils.h"

struct rdb_session *g_rdb_session;

const TRdbNameList g_rdbNameList[] =
{


	///MDA
	{MDA_Add_IF,	1,  RDB_VAR_SUBCRIBE|RDB_NO_VAR,	""},
	{MDA_Changed_IF,1,  RDB_VAR_SUBCRIBE|RDB_NO_VAR,	""},
	{DOT1AG_Changed,	1,	RDB_VAR_SUBCRIBE,	""}, //W
	{MDA_GetStats,	1,	0,	"0"}, //W
	{MDA_Attached_IF,	1,	0,	""}, //W
	{MDA_MdLevel,	1,	0,	"2"}, //W
	{MDA_PrimaryVid,1,	0,	"0"}, //W
	{MDA_MDFormat,	1,	0,	"1"}, //W
	{MDA_MdIdType2or4,	1,	0,	"MD1"}, //W
	{MDA_MaFormat,	1,	0,	"2"}, //W
	{MDA_MaIdType2,	1,	0,	"MA1"}, //W
	{MDA_MaIdNonType2,	1,	0,	"0"}, //W
#ifdef ETH_CCM
	{MDA_CCMInterval,		1,	0,	"4"}, //W 1s
	{MDA_lowestAlarmPri,	1,	0,	"2"}, //W Only DefMACstatus, DefRemoteCCM, DefErrorCCM, and DefXconCCM
	{MDA_errorCCMdefect,	1,	0,	"0"}, //-
	{MDA_xconCCMdefect,		1,	0,	"0"}, //-
	{MDA_CCMsequenceErrors,	1,	0,	"0"}, //-
	{MDA_fngAlarmTime,		1,	0,	"2500"}, //W
	{MDA_fngResetTime,		1,	0,	"10000"}, //W
	{MDA_someRMEPCCMdefect,	1,	0,	"0"}, //-
	{MDA_someMACstatusDefect,	1,	0,	"0"}, //-
	{MDA_someRDIdefect,		1,	0,	"0"}, //-
	{MDA_highestDefect,		1,	0,	"0"}, //-
	{MDA_fngState,			1,	0,	"0"}, //-
#endif



	{MDA_AVCID,		1,	RDB_VAR_SET_IF,	""}, //W

	{MDA_Status,	1,	0,	""}, //-

	///LMP
	{MDA_PeerMode,	1,	0,	"0"}, //W
	{LMP_MEPactive,	1,	0,	"0"}, //W

	{LMP_mpid,		1,	0,	"1"}, //W
	{LMP_direction,	1,	0,	"1"}, //W
	{LMP_port,		1,	0,	"2"}, //W
	{LMP_vid,		1,	0,	"0"}, //W
	{LMP_vidtype,	1,	0,	"0"}, //W

	{LMP_macAdr,	1,	0,	""}, //W
	{LMP_CoS,	1,	0,	"0"}, //W


#ifdef ETH_CCM
	{LMP_CCMenable,			1,	0,	"0"}, //W
	{LMP_CCIsentCCMs, 		1, 0, "1"}, //R
	{LMP_xconnCCMdefect,	1,	0,	"0"}, //-
	{LMP_errorCCMdefect,	1,	0,	"0"}, //-
	{LMP_CCMsequenceErrors,	1,	0,	"0"}, //-
#endif
	{LMP_TxCounter, 		1, 0, "0"}, //R
	{LMP_RxCounter, 		1, 0, "0"}, //R

	{LMP_Status, 			1, 0,""},

	{LMP_LBRsInOrder,		1,	0,	"0"}, //-
	{LMP_LBRsOutOfOrder,	1,	0,	"0"}, //-
	{LMP_LBRnoMatch,		1,	0,	"0"}, //-
	{LMP_LBRsTransmitted,	1,	0,	"0"}, //-
	{LMP_LTRsUnexpected,	1,	0,	"0"}, //-
	///RMP

	///LTM
	{LTM_send,		1,	0,	"0"}, //W
	{LTM_rmpid,		1,	0,	"0"}, //W
	{LTM_destmac,	1,	0,	""}, //W
	{LTM_flag,		1,	0,	"0"}, //W
	{LTM_ttl,		1,	0,	"64"}, //W
	{LTM_timeout,	1,	0,	"5000"}, //W
	{LTM_LTMtransID,1,	0,	"1"}, //-
	{LTM_Status,	1, 0, ""},

	///LTR
	{LTR_ltmTransId,1,	0,	"0"}, //-
	{LTR_rmpid,		1,	0,	""}, //-	
	{LTR_srcmac,	1,	0,	""}, //-
	{LTR_flag,		1,	0,	""}, //-
	{LTR_relayaction,	1,	0,	""}, //-
	{LTR_ttl,		1,	0,	""}, //-

	///LBM
	{LBM_LBMsToSend,1,	0,	"0"}, //W
	{LBM_rmpid,		1,	0,	"0"}, //W
	{LBM_destmac,	1,	0,	""}, //W
	{LBM_timeout,	1,	0,	"5000"}, //W
	{LBM_rate,		1,	0,	"1000"}, //W
	{LBM_LBMtransID,1,	0,	"1"}, //-
	{LBM_TLVDataLen, 1, 0, "0"}, //W
	{LBM_Status,	1, 0, ""},

	//{TLV_SENDERID ,1, 0,""},	//readonly
	//{TLV_ORGSPEC,1, 0, ""},	//readonly
#ifdef NCI_Y1731

	//Y1731 MDA


	
	{Y1731_Mda_Enable, 1, 0, "0"}, //W
	//{A_Y1731_Mda_AVCID, 1, 0, ""}, //W
	{Y1731_Mda_MegLevel, 1, 0, "2"}, //W
	{Y1731_Mda_MegId, 1, 0, ""}, //W
	{Y1731_Mda_MegIdFormat, 1, 0, "32"}, //W
	{Y1731_Mda_MegIdLength, 1, 0, "13"}, //W
	//{A_Y1731_Mda_someRDIdefect, 1, 0, "0"}, //R
	{Y1731_Mda_ALMSuppressed, 1, 0, "0"}, //R
	//{A_Y1731_Mda_Status, 1, 0, ""}, //R






	//Y1731 LMP
	
	{Y1731_Lmp_Cos, 1, 0, "0"}, //W
	//{A_Y1731_Lmp_Status, 1, 0, ""}, //R
	
	//Y1731 DMM
	{Y1731_Dmm_rmpid, 1, 0, "0"}, //W
	{Y1731_Dmm_destmac, 1, 0, ""}, //W
	{Y1731_Dmm_timeout, 1, 0, "5000"}, //W
	{Y1731_Dmm_Send, 1, 0, "0"}, //W
	{Y1731_Dmm_Rate, 1, 0, "500"}, //W
	{Y1731_Dmm_Type, 1, 0, "0"}, //W
	{Y1731_Dmm_TLVDataLen, 1, 0, "0"}, //W
	{Y1731_Dmm_Dly, 1, 0, ""}, //R
	{Y1731_Dmm_DlyAvg, 1, 0, ""}, //R
	{Y1731_Dmm_DlyMin, 1, 0, ""}, //R
	{Y1731_Dmm_DlyMax, 1, 0, ""}, //R
	{Y1731_Dmm_Var, 1, 0, ""}, //R
	{Y1731_Dmm_VarAvg, 1, 0, ""}, //R
	{Y1731_Dmm_VarMin, 1, 0, ""}, //R
	{Y1731_Dmm_VarMax, 1, 0, ""}, //R
	{Y1731_Dmm_Count, 1, 0, "0"}, //R
	{Y1731_Dmm_Status, 1, 0, ""}, //R


	{Y1731_Slm_rmpid, 1, 0, "0"}, //W
	{Y1731_Slm_destmac, 1, 0, ""}, //W
	{Y1731_Slm_timeout, 1, 0, "5000"}, //W
	{Y1731_Slm_Send, 1, 0, "0"}, //W
	{Y1731_Slm_Rate, 1, 0, "500"}, //W
	{Y1731_Slm_TestID, 1, 0, "1"}, //W
	{Y1731_Slm_TLVDataLen, 1, 0, "0"}, //W
	{Y1731_Slm_Curr, 1, 0, ""}, //R
	{Y1731_Slm_Accum, 1, 0, ""}, //R
	{Y1731_Slm_Ratio, 1, 0, ""}, //R
	{Y1731_Slm_Count, 1, 0, ""}, //R
	{Y1731_Slm_Status, 1, 0, ""}, //R

#endif


#ifdef _DEBUG
	{DOT1AG_TEST,		1,	0,	""},
	{DOT1AG_TEST_MEP,	1,	0,	""},
#endif
#ifdef _RDB_RPC
	{DOT1AG_CMD_COMMAND,1, RDB_VAR_SUBCRIBE|RDB_NO_VAR, ""},
	{DOT1AG_CMD_STATUS,	1, RDB_NO_VAR|RDB_NO_VAR, ""},
	{DOT1AG_CMD_MESSAGE,1, RDB_NO_VAR|RDB_NO_VAR, ""},
#endif
	{DOT1AG_INDEX,	1,  RDB_NO_VAR|RBD_FLAG_PERSIST,	""}, 

	{RMP_index,		1,	RDB_VAR_SUBCRIBE|RBD_FLAG_PERSIST,	""}, 



    {0,},

};


const TRdbNameList g_rmpNameList[] =
	{
	{RMP_mpid, 1, 0, "0"}, //W
	{RMP_ccmDefect, 1, 0, "0"}, //-
	{RMP_lastccmRDI, 1, 0, "0"}, //-
	{RMP_lastccmPortState, 1, 0, "0"}, //-
	{RMP_lastccmIFStatus, 1, 0, "0"}, //-
	{RMP_lastccmSenderID, 1, 0, "0"}, //-
	{RMP_lastccmmacAddr, 1, 0, ""}, //-
	{RMP_TxCounter, 1, 0, "0"}, //R
	{RMP_RxCounter, 1, 0, "0"}, //R
	{RMP_Curr, 1, 0, ""}, //R
	{RMP_Accum, 1, 0, ""}, //R
	{RMP_Ratio, 1, 0, ""}, //R
    {0,},

};
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

/// advanced RDB function with prefix in first '.'
static void get_p1_buf(char *buf, const char* rdbname, int i)
{
	
	if(i > 0)
	{
		const char *p1, *p2;
		buf[0] =0;
		p1 =  rdbname;
		p2 = strchr(p1, '.');
		if(p2)
		{
			int len;
			p2++;
			len = p2-p1;
			strncat(buf,p1, len);
			sprintf(&buf[len], "%d.%s", i, p2);
			return ;
		}

	}
	strcpy(buf, rdbname);
}


// advanced RDB function with prefix in first '.' and second '.'
/// example: dot1ag.rmp.lastccmmacaddr=>dot1ag.1.rmp.2.lastccmmacaddr
static  void get_p1_2_buf(char *buf, const char* rdbname, int i, int j)
{
	if(i > 0 || j > 0 )
	{
		const char *p1, *p2;
		int test[2]={i,j};
		int n;
		buf[0] =0;
		p1= p2 = rdbname;
		for(n =0; n<2;n++)
		{
			p2 = strchr(p1, '.');
			if(p2)
			{
				int len;
				p2++;
				len =p2-p1;
				strncat(buf, p1, len);
				if( test[n] )
				{
					char tmp[20];
					sprintf(tmp, "%d.", test[n] );
					strcat(buf, tmp);
				}
			}
			else
			{
				strcpy(buf,rdbname);
				return;
			}
			p1=p2;
		}
		strcat(buf, p2);
		return;
	}

	strcpy(buf,rdbname);
}

////////////////////////////////////////////////////////////////////////////////
///initilize rdb variables inside of session
/// id -1, 0, >0
/// -1		-- create rdb with RDB_NO_VAR flags
///  >=0	-- create rdb without RDB_NO_VAR flags
int create_rdb_variables(int id)
{
	const TRdbNameList* pNameList= g_rdbNameList;

	for(; pNameList->szName; pNameList ++)
	{
		char value[MAX_RDB_NAME_SIZE];
		char name[MAX_RDB_VALUE_SIZE];
		int len = sizeof(value);
		//1) not create
		if (pNameList->bcreate ==0)  continue;

		if(pNameList->attribute&RDB_NO_VAR)
		{
			// non-var rdb with var ID
			if(id >= 0) continue;
		}
		else
		{
			// var rdb with no var ID
			if(id < 0) continue;
		}
		get_p1_buf(name, pNameList->szName,id);
		if (rdb_get(g_rdb_session, name, value, &len) != 0){
			int flags = CREATE;
			if (pNameList->attribute&RBD_FLAG_PERSIST) flags |= PERSIST;
			NTCLOG_DEBUG("create '%s'", name);
			if(rdb_create(g_rdb_session, name, pNameList->szValue, 1, flags, DEFAULT_PERM)<0)
			{
				NTCLOG_ERR("failed to create '%s'", name);
			}
		}

	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
int create_rmp_rdb_variables(int id1, int id2)
{

	const TRdbNameList* pNameList= g_rmpNameList;

	for(; pNameList->szName; pNameList++)
	{
		char value[MAX_RDB_NAME_SIZE];
		char name[MAX_RDB_VALUE_SIZE];
		int len = sizeof(value);
		//1) not create
		if (pNameList->bcreate ==0)  continue;

		get_p1_2_buf(name, pNameList->szName, id1, id2);
		if (rdb_get(g_rdb_session, name, value, &len) != 0)
		{
			int flags= CREATE;
			if (pNameList->attribute&RBD_FLAG_PERSIST) flags |= PERSIST;

			NTCLOG_DEBUG("create '%s'", name);
			if(rdb_create(g_rdb_session, name, pNameList->szValue, 1, CREATE, ALL_PERM)<0)
			{
				NTCLOG_ERR("failed to create '%s'", name);
			}
		}

    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// open rdb, initilize global rdb variables and subscrible
int rdb_init_global()
{
	// create variable without var
    create_rdb_variables(-1);
	subscribe(-1);
    return 0;
}
// close rdb, delete global rdb variables
void rdb_end_global(int remove_rdb)
{
	if(remove_rdb)
	{
		// delete rdb without var
		delete_rdb_variables(-1);
	}
}


///////////////////////

/// id -1, 0, >0
/// -1		-- delete rdb with RDB_NO_VAR flags
//   >=0 	-- delete rdb without RDB_NO_VAR flags

void delete_rdb_variables(int id)
{
	const TRdbNameList* pNameList=  g_rdbNameList;
	while(pNameList->szName)
	{
		char name[MAX_RDB_NAME_SIZE];
		if(pNameList->attribute&RDB_NO_VAR)
		{
			if(id >= 0 ) goto lab_next_item;
		}
		else
		{
			if( id < 0 ) goto lab_next_item;
		}
		get_p1_buf(name, pNameList->szName,id);

		if(pNameList->bcreate && (pNameList->attribute&RDB_NO_REMOVE) ==0)
		{
			NTCLOG_DEBUG("delete %s",name);
			rdb_delete(g_rdb_session, name);
		}

lab_next_item:

		pNameList++;
	}




}

void delete_rmp_rdb_variables(int id1, int id2)
{
	const TRdbNameList* pNameList=  g_rmpNameList;
	while(pNameList->szName)
	{
		char name[MAX_RDB_NAME_SIZE];

		get_p1_2_buf(name, pNameList->szName, id1, id2);

		if(pNameList->bcreate )
		{
			NTCLOG_DEBUG("delete %s", name);
			rdb_delete(g_rdb_session, name);
		}
		pNameList++;
	}

}


////////////////////////////////////////////////////////////////////////////////
/// id -1, 0, >0
/// -1		-- subscrible rdb with RDB_NO_VAR flags
///  >=0	-- subscrible rdb without RDB_NO_VAR flags
int subscribe(int id)
{
	const TRdbNameList* pNameList= g_rdbNameList;

	while(pNameList->szName)
	{
		if(pNameList->attribute&RDB_VAR_SUBCRIBE)
		{
			char name[MAX_RDB_NAME_SIZE];
			if(pNameList->attribute&RDB_NO_VAR)
			{
				if(id >= 0 ) goto lab_next_item;

			}
			else
			{
				if( id < 0 ) goto lab_next_item;
			}
			get_p1_buf(name, pNameList->szName,id);

			//NTCLOG_DEBUG("subscribing %s",pNameList->szName);
			if (rdb_subscribe(g_rdb_session, name) <  0)
			{
				NTCLOG_ERR("failed to subscribe to '%s'!", name);
			}
		}
lab_next_item:

		pNameList++;
	}

	//handle_rdb_events();

	return 0;
}


/// basic RDB function
////////////////////////
// check whether RDB exist or not
// 0: -- rdb not exist
// 1  --- rdb exist
int rdb_exist(const char *rdb_name)
{
	return rdb_get(g_rdb_session, rdb_name, NULL, NULL) >=0;
}

int rdb_get_boolean(const char* rdbname, int *value)
{
	char buf[MAX_RDB_VALUE_SIZE];
	int len = sizeof(buf);
	int retValue = rdb_get(g_rdb_session, rdbname, buf, &len);
	if(retValue)
	{
		return retValue;
	}
	*value = buf[0] !='0' && buf[0] !=0;
	return 0;
}
////////////////////////////////////////
int rdb_set_boolean(const char* rdbname, int value)
{
	char buf[2];
	int retValue;
	buf[0] =value !=0?'1':'0';
	buf[1] =0;
	retValue = rdb_set_string(g_rdb_session, rdbname, buf);
	if(retValue)
	{
		return retValue;
	}
	return 0;
}

//////////////////////////////////////////
int rdb_get_sint(const char* rdbname, int *value)
{
    return rdb_get_uint(rdbname, (unsigned int*) value);
}

//////////////////////////////////////////////
int rdb_set_int(const char* rdbname, int value)
{
	char buf[32];
	int retValue;
	sprintf(buf, "%d", value);
	retValue = rdb_set_string(g_rdb_session,  rdbname, buf);
	if(retValue)
	{
		return retValue;
	}
	return 0;
}
//////////////////////////////////////////
int rdb_get_uint(const char* rdbname, unsigned int *value)
{
	char buf[MAX_RDB_VALUE_SIZE];
	int retValue;
	int len = sizeof(buf);
	retValue = rdb_get(g_rdb_session, rdbname, buf, &len);
	if(retValue)
	{
		return retValue;
	}
	buf[31]=0;
	*value = Atoi(buf, NULL);
	return 0;
}
//////////////////////////////////////////////
int rdb_set_uint(const char* rdbname, unsigned int value)
{
	char buf[32];
	int retValue;
	sprintf(buf, "%u", value);
	retValue = rdb_set_string(g_rdb_session,  rdbname, buf);
	if(retValue)
	{
		return retValue;
	}
	return 0;
}

int rdb_get_mac(const char* rdbname, unsigned char *mac)
{
	char str[MAX_RDB_VALUE_SIZE];
	int err;
	int len = MAX_RDB_VALUE_SIZE;
	err = rdb_get(g_rdb_session, rdbname, str, &len);
	if(err ==0)
	{
		//NTCLOG_DEBUG("%s =%s", rdbname, str);

		if(!str2MAC(mac, str))
		{
			err =-1;
		}
	}
	return err;

}

int rdb_set_mac(const char* rdbname, const unsigned char* mac)
{
	char str[20];

	MAC2str(str, mac);
#ifdef _DEBUG
	NTCLOG_DEBUG("%s => %s", rdbname, str);
#endif
	return rdb_set_string(g_rdb_session,  rdbname, str);

}

////////////////////////
/// advanced RDB function with prefix in first '.'
/// dot1ag.1.mda.peermode => dot1ag.mda.peermode, return 1, or 0 not(found)
int strip_p1(char* buf, const char *orig_name, int max_session)
{
	int id=0;
	const char *p1, *p2;
	char *pEnd;
	p1 = orig_name;
	p2 = strchr(p1, '.');
	if(p2)
	{
		p2++;
		buf[0] =0;
		strncat(buf, p1, p2-p1);
		id = strtol(p2, &pEnd, 10);
		
		if(id >= 0  && id < max_session && *pEnd == '.')
		{
			pEnd++;// skip the .
			
			strcat (buf, pEnd);
			return id;
		}
	}
	strcpy(buf, orig_name);
	return id;
}




////////////////////////
int rdb_get_p1_boolean(const char* rdbname, int i, int *value)
{
	char buf[MAX_RDB_NAME_SIZE];
	get_p1_buf(buf, rdbname, i);
	return rdb_get_boolean(buf, value);
}
///////////////////////////////////
int rdb_set_p1_boolean(const char* rdbname, int i, int value)
{
	char buf[MAX_RDB_NAME_SIZE];
	get_p1_buf(buf, rdbname, i);
	return rdb_set_boolean(buf, value);
}

//////////////////////////////////////////////
int rdb_get_p1_int(const char* rdbname, int i, int *value)
{
	char buf[MAX_RDB_NAME_SIZE];
	get_p1_buf(buf, rdbname, i);
	return rdb_get_sint(buf, value);
}
int rdb_set_p1_int(const char* rdbname, int i, int value)
{
	char buf[MAX_RDB_NAME_SIZE];
	get_p1_buf(buf, rdbname, i);
    return rdb_set_int(buf, value);
}

//////////////////////////////////////////////
int rdb_get_p1_uint(const char* rdbname, int i, unsigned int *value)
{
	char buf[MAX_RDB_NAME_SIZE];
	get_p1_buf(buf, rdbname, i);
	return rdb_get_uint(buf, value);
}

int rdb_set_p1_uint(const char* rdbname, int i, unsigned int value)
{
    char buf[MAX_RDB_NAME_SIZE];
	get_p1_buf(buf, rdbname, i);
    return rdb_set_uint(buf, value);
}


//////////////////////////////////////////////
int rdb_get_p1_str(const char* rdbname, int i, char* str, int len)
{
	char name[MAX_RDB_NAME_SIZE];
	get_p1_buf(name, rdbname, i);
	return rdb_get(g_rdb_session, name, str, &len);
}
int rdb_set_p1_str(const char* rdbname, int i, const char* str)
{
	char name[MAX_RDB_NAME_SIZE];
	get_p1_buf(name, rdbname, i);
	return rdb_set_string(g_rdb_session, name, str);
}

//////////////////////////////////////////////

int rdb_set_p1_2str(const char* rdbname, int i, const char* str1, const char* str2)
{
	char msg[MAX_RDB_VALUE_SIZE];
	strcpy(msg, str1);
	strcat(msg, str2);
	return rdb_set_p1_str(rdbname, i, msg);
}


//////////////////////////////////////////////
int rdb_get_p1_mac(const char* rdbname, int i, unsigned char* mac)
{
	char name[MAX_RDB_NAME_SIZE];
	get_p1_buf(name, rdbname, i);
	return rdb_get_mac(name, mac);
}

int rdb_set_p1_mac(const char* rdbname, int i, const unsigned char* mac)
{
	char name[MAX_RDB_NAME_SIZE];
	get_p1_buf(name, rdbname, i);
	return rdb_set_mac(name, mac);

}


//////////////////////////////////////////
// advanced RDB function with prefix in first '.' and second '.'
/// example: dot1ag.rmp.lastccmmacaddr=>dot1ag.1.rmp.2.lastccmmacaddr
int rdb_set_p1_2_boolean(const char* rdbname, int i, int j, int value)
{
	char name[MAX_RDB_NAME_SIZE];
	get_p1_2_buf(name, rdbname, i, j);
	return rdb_set_boolean(name, value);
}


//////////////////////////////////////////
int rdb_set_p1_2_str(const char* rdbname, int i, int j, const char* str)
{
	char name[MAX_RDB_NAME_SIZE];
	get_p1_2_buf(name, rdbname, i, j);
	return rdb_set_string(g_rdb_session,  name, str);
}


//////////////////////////////////////////
int rdb_get_p1_2_int(const char* rdbname, int i, int j, int *value)
{
	char name[MAX_RDB_NAME_SIZE];
	get_p1_2_buf(name, rdbname, i, j);
	return rdb_get_sint(name, value);
}

//////////////////////////////////////////
int rdb_set_p1_2_int(const char* rdbname, int i, int j, int value)
{
	char name[MAX_RDB_NAME_SIZE];
	get_p1_2_buf(name, rdbname, i, j);
	return rdb_set_int(name, value);

}

//////////////////////////////////////////
int rdb_set_p1_2_uint(const char* rdbname, int i, int j, unsigned int value)
{
	char name[MAX_RDB_NAME_SIZE];
	get_p1_2_buf(name, rdbname, i, j);
	return rdb_set_uint(name, value);

}


//////////////////////////////////////////
int rdb_set_p1_2_mac(const char* rdbname, int i, int j, const unsigned char* mac)
{
	char name[MAX_RDB_NAME_SIZE];
	get_p1_2_buf(name, rdbname, i, j);
	return rdb_set_mac(name, mac);

}

//////////////////////////////////////////////
//get int array from rdb
int rdb_get_p1_uint_array(const char* rdbname, int i, unsigned int *a, int maxlen)
{
	char data[MAX_RDB_VALUE_SIZE];
	int err;
	err =rdb_get_p1_str(rdbname, i, data, MAX_RDB_VALUE_SIZE);
	if(err ==0)
	{
		int n=0;
		char *token=strtok(data, ",");
		while(token && n < maxlen)
		{
			a[n++] = atoi(token);
			token=strtok(NULL, ",");
		}
		return n;
	}
	return err;
	
}
int rdb_set_p1_uint_array(const char* rdbname, int i, const unsigned int* a, int len)
{
	char data[MAX_RDB_VALUE_SIZE];
	int n;
	int l=0;
	int err =0;
	for(n =0; n < len; n++)
	{
		sprintf(&data[l], "%d,", a[n] );
		l = strlen(data);
		if(l > (MAX_RDB_VALUE_SIZE-10)) break;
		
	}
	if(l>0 && data[l-1] ==',') data[l-1]=0;
	err = rdb_set_p1_str(rdbname, i , data);
	if(err == 0)
	{
		return n;
	}
	return err;
}


//////////////////////////////////////////
// advanced RDB function with read a number list
/// example: "1,2,3,4"=>[0]=1,[1]=2,[2]=3,[3]=4
///rdbname -- rdbname, may need a var
///i       -- variable in rdbname, if it is >0
// pIndex	-- index array 
// max_number -- maxium allown index number 
// -1 --- rdb does not exist
// >0 --- number of in pIndex
int rdb_get_index_list(const char* rdbname, int i, int *pIndex, int max_number)
{
	char buf[MAX_RMP_OBJ_NUM*5];
	int err;
	if(i >0)
	{
		err= rdb_get_p1_str(rdbname, i, buf, MAX_RMP_OBJ_NUM*5);
	}
	else
	{
		
		err= rdb_get_str(rdbname, buf, MAX_RMP_OBJ_NUM*5);
	}
	
	if(err ==0)
	{
		int n=0;
		char *token=strtok(buf, ",");
		while(token && n < max_number)
		{
			pIndex[n] = atoi(token);
			if(pIndex[n] > 0 && pIndex[n] < max_number) n++;

			token=strtok(NULL, ",");
		}
		return n;
	}
	return err;
	
}
/// example: [0]=1,[1]=2,[2]=3,[3]=4 => "1,2,3,4"
///rdbname -- rdbname, may need a var
///i       -- variable in rdbname, if it is >0
// pIndex	-- index array 
// max_number -- maxium allown index number 
// -1 --- rdb does not exist
// ==0 --- success
int rdb_set_index_list(const char* rdbname, int i, int *pIndex, int number)
{
	char buf[MAX_RMP_OBJ_NUM*5];
	int n;
	int l=0;
	int err =0;
	for(n =0; n < number; n++)
	{
		sprintf(&buf[l], "%d,", pIndex[n] );
		l = strlen(buf);
		if(l > (MAX_RMP_OBJ_NUM*5-10) ) break;
	}
	
	if(l>0 && buf[l-1] ==',') buf[l-1] = 0;
	
	if(i >0)
	{
		err= rdb_set_p1_str(rdbname, i, buf);
	}
	else
	{
		err= rdb_set_str(rdbname, buf);
	}

	return err;
}
/*
look for avc index for matched its avc.x.avcid
$param avcid -- to search
$ -1 -- error
$ 0  -- not found
$>0   -- the matched index
*/
int get_avc_index(const char *avcid)
{
	char value[MAX_RDB_VALUE_SIZE];
	int len = sizeof(value);
	int err;
	char *token;
	err = rdb_get(g_rdb_session, "avc._index", value, &len);
	if(err != 0) return -1;
	token=strtok(value, ",");
	while(token){
		char name_buf[MAX_RDB_NAME_SIZE];
		char id_buf[MAX_AVCID_LEN+1];
		int index = atoi(token);
		len = sizeof(id_buf);
		snprintf(name_buf, sizeof(name_buf), "avc.%d.avcid", index);
		err = rdb_get(g_rdb_session, name_buf, id_buf, &len);
		if(err != 0) return -1;
		if(strcmp(id_buf, avcid) ==0) return index;
		token=strtok(NULL, ",");
	}
	return 0;
}

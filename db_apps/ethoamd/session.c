#include <string.h>
#include <ctype.h>
#include "rdb_comms.h"
#include "session.h"
#include "utils.h"

//#define MY_MDA_DEVNAME		"br0"			/* device Name */
//#define MY_MDA_DEVNAME		"eth1"			/* device Name */
//#define MY_MDA_DEVNAME2		"eth0"			/* device Name */
//#define MY_MDA_DEVMAC		"00:15:17:63:B0:74"	/* device MAC */
//2#define MY_MDA_DEVMAC		"1c:6f:65:31:15:ed"	/* device MAC */
//2#define MY_MDA_DEVMAC2		"1c:6f:65:31:15:dd"	/* device MAC */
#define MY_MDA_DEVMAC		"00:10:13:a5:50:3c"	/* device MAC */
#define MY_MDA_DEVMAC2		"00:10:13:a5:50:33"	/* device MAC */

#define MY_MDA_LEVEL		1			/* MDA Level */

//#define MY_MDA_PORTMAC1		"00:15:17:63:B0:74"	/* bridge-port MAC */
//#define MY_MDA_PORTMAC2		"00:15:17:63:B0:75"	/* bridge-port MAC */
//#define MY_MDA_PORTMAC1		"1c:6f:65:31:15:dd"	/* bridge-port MAC */
//#define MY_MDA_PORTMAC2		"1c:6f:65:31:15:ed"	/* bridge-port MAC */
//#define MY_MDA_PORTMAC1		"1c:6f:65:31:15:ed"	/* bridge-port MAC */
//#define MY_MDA_PORTMAC2		"1c:6f:65:31:15:30"	/* bridge-port MAC */
#define MY_MDA_PORTMAC1		"1c:6f:65:31:16:04"	/* bridge-port MAC */
#define MY_MDA_PORTMAC2		"1c:6f:65:31:16:05"	/* bridge-port MAC */


/* LMP and RMP identifier values must be between the 1 and 8191.
 */
#define MY_MDA_LMPIDENT1	100			/* ident of LMP 1 */
#define MY_MDA_LMPIDENT2	200			/* ident of LMP 2 */

#define MY_MDA_RMPIDENT1	10			/* ident of RMP 1 */
#define MY_MDA_RMPIDENT2	20			/* ident of RMP 2 */


#define ORG_SPECIFIC_DATA_TYPE 0xff // cos

#define LMM_TRANS_ID 1000
#define DMM_TRANS_ID 2000
#define SLM_TRANS_ID 3000

int open_session(Session **pSession, int sessionID, TConfig *pConfig)
{
	*pSession = malloc(sizeof(Session));
	if(*pSession ==0) return -1;
	memset(*pSession, 0, sizeof(Session));

    (*pSession)->m_config = pConfig;
    (*pSession)->m_node = sessionID;
    create_rdb_variables(sessionID);

    subscribe((*pSession)->m_node);

	(*pSession)->m_if_bridge = 1;
	(*pSession)->m_removing_if = 0;
	(*pSession)->m_dmm_data.runID=DMM_TRANS_ID;
	(*pSession)->m_lmm_data.runID=LMM_TRANS_ID;
	(*pSession)->m_slm_data.runID=SLM_TRANS_ID;


	return 0;
}
void close_all_session( TConfig *pConfig)
{
	int i;
	if(pConfig->m_remove_rdb)
	{
		for(i =0; i< pConfig->m_max_session; i ++ )
		{
			Session *pSession = g_session[i];
			if (pSession)
			{
				RMP_del_all(pSession);
				delete_rdb_variables(pSession->m_node);
				free(pSession);
			}
		}
	}

}

int rdb_set_2str_all(TConfig *pConfig, const char* rdbname, const char *str1, const char *str2)
{
	int sessionID;
	for(sessionID =0; sessionID < pConfig->m_max_session; sessionID++)
	{
		Session *pSession = g_session[sessionID];
		if(pSession == NULL) continue;
		if(str2)
		{
			RDB_SET_P1_2STR(MDA_Status, str1, str2);
		}
		else
		{
			RDB_SET_P1_STR(MDA_Status, str1);
		}
    }
    return 0;
}


/*
	load mda config from rdb
*/
int load_mda(Session *pSession)
{
	int err =0;
	mda_data		*m= &pSession->m_mda_data;
#ifdef NCI_Y1731
	mda_state		*ms= &pSession->m_mda_state;
#endif

	TRY_RDB_GET_P1_UINT(MDA_MdLevel, &m->MdLevel);
	TRY_RDB_GET_P1_UINT(MDA_PrimaryVid, &m->PrimaryVID);
	TRY_RDB_GET_P1_UINT(MDA_MDFormat, &m->MDFormat);
	TRY_RDB_GET_P1_STR(MDA_MdIdType2or4, m->MdIdType2or4, MDA_ID_TYPE_LEN+1);

#ifdef NCI_Y1731
	TRY_RDB_GET_P1_INT(Y1731_Mda_Enable, &ms->Y1731_MDA_enable);
	TRY_RDB_GET_P1_STR(Y1731_Mda_MegId, m->MegId, MDA_ID_TYPE_LEN+1);
	TRY_RDB_GET_P1_UINT(Y1731_Mda_MegLevel, &m->MegLevel);
	TRY_RDB_GET_P1_UINT(Y1731_Mda_MegIdFormat, &m->MegIdFormat);
	TRY_RDB_GET_P1_UINT(Y1731_Mda_MegIdLength, &m->MegIdLength);
#endif

	TRY_RDB_GET_P1_UINT(MDA_MaFormat, &m->MaFormat);
	TRY_RDB_GET_P1_STR(MDA_MaIdType2, m->MaIdType2, MDA_ID_TYPE_LEN+1);
	TRY_RDB_GET_P1_UINT(MDA_MaIdNonType2, &m->MaIdNonType2);

	TRY_RDB_GET_P1_STR(MDA_AVCID, m->avcid, MAX_AVCID_LEN+1);

lab_err:
	return err;
}

/* lookup active avc bridge interface name
$ 0 -- avc is active and its interface exist
$ -1 -- failed
*/
int get_session_if(Session *pSession)
{
	int err;
	int avc_index;
	int session_id;
	unsigned int trunk_vid;
	unsigned int unid;
	char name_buf[MAX_RDB_NAME_SIZE];
	char value_buf[MAX_RDB_NAME_SIZE];
	struct stat   stat_buf;
	//get matched interface name
	pSession->m_if_name[0] = 0;
	pSession->m_insert_priority_tag = 0;
	if(pSession->m_removing_if) return -1;
	avc_index = get_avc_index(pSession->m_mda_data.avcid);
	if(avc_index <= 0) return -1;
	// check both avc.x.status and avc.x.br
	sprintf(name_buf, "avc.%d.status", avc_index);
	err = rdb_get_str(name_buf, value_buf, MAX_RDB_NAME_SIZE);
	if(err != 0 || strcmp(value_buf, "Up") != 0) return -1;

	// get interface name
	sprintf(name_buf, "avc.%d.trunk_vid", avc_index);
	err = rdb_get_uint(name_buf, &trunk_vid);
	if(err != 0 || trunk_vid <= 0) return -1;
	sprintf(pSession->m_if_name, "br%d", trunk_vid);

	// check the interface exists
	sprintf(name_buf, "/sys/class/net/%s", pSession->m_if_name);
	if(stat (name_buf, &stat_buf) != 0)
	  return -1;
	// get unid tagging
	sprintf(name_buf, "avc.%d.unid", avc_index);
	rdb_get_uint(name_buf, &unid);
	sprintf(name_buf, "unid.%d.tagging", unid);
	err = rdb_get_str(name_buf, value_buf, MAX_RDB_NAME_SIZE);
	if(err == 0){
		if(strcmp(value_buf, "PriorityTagged") == 0){
			pSession->m_insert_priority_tag = 1;
		}
	}
	// if bridge interface is locked in other session, please don't lock it
	for(session_id =0; session_id < MAX_SESSION; session_id++){
		Session *pS = g_session[session_id];
		if(pS && pS != pSession && pS->m_mda_state.PeerMode && strcmp(pSession->m_if_name, pS->m_if_name)==0){
			return -1;
		}
	}
	// lock interface of AVC
	RDB_SET_P1_STR(MDA_Attached_IF, pSession->m_if_name);
	return 0;
}

/*
unlock the interface of AVC
$0 -- operation success
$-1 -- error
*/
int unlock_avc_if(Session *pSession)
{
	int err;
	err = RDB_SET_P1_STR(MDA_Attached_IF, "");
	if (err != 0)
		return -1;
	return 0;
}

// check whether mda config changed
// >0 -- changed
// 0 -- not changed
int check_mda_config(Session *pSession)
{
	int err;
	unsigned int tmp;

	const char *rdb_name=0;
	char buf[MDA_ID_TYPE_LEN+1];
	mda_data		*m= &pSession->m_mda_data;

#ifdef NCI_Y1731
	mda_state		*ms= &pSession->m_mda_state;
#endif

	RDB_CMP_P1_UINT(MDA_MdLevel, tmp, m->MdLevel);
	RDB_CMP_P1_UINT(MDA_PrimaryVid, tmp, m->PrimaryVID);

	RDB_CMP_P1_UINT(MDA_MDFormat, tmp , m->MDFormat);
	RDB_CMP_P1_STR(MDA_MdIdType2or4, buf, m->MdIdType2or4, MDA_ID_TYPE_LEN+1);

#ifdef NCI_Y1731
	RDB_CMP_P1_UINT(Y1731_Mda_Enable, tmp,  ms->Y1731_MDA_enable);
	RDB_CMP_P1_STR(Y1731_Mda_MegId, buf, m->MegId, MDA_ID_TYPE_LEN+1);
	RDB_CMP_P1_UINT(Y1731_Mda_MegLevel, tmp, m->MegLevel);

#endif
	RDB_CMP_P1_STR(MDA_AVCID, buf, m->avcid, MAX_AVCID_LEN+1);

	NTCSLOG_DEBUG("MDA Config no change");
	return 0;

lab_value_changed:
	NTCSLOG_DEBUG("MDA Config changed, %s", rdb_name);
	return 1;

lab_err:
	return 0;
}


/// form error message eg:
/// can not set parameter 100:"transid"
void build_err_msg(char *buf, const char *msg, int id, const char *key_name)
{
	char *p1;
	const char *p2;
	sprintf(buf, "%s \"",msg);
	p1 = buf+strlen(buf);

	p2 = strchr(key_name, '_');
	if(p2)
	{
		p2++;
		if(p2[0] == 'N' && p2[1] == 'E'&& p2[2] == 'X'&& p2[3] == 'T')
		{
			p2+=4;
		}
	}
	else
	{
	p2 = key_name;
	}


	while(*p2)
	{
		*p1++ = tolower(*p2);
		p2++;
	}
	*p1++ ='\"';
	*p1 =0;
}


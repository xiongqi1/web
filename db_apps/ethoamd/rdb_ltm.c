

#include "rdb_comms.h"
#include "ethlctrl.h"
#include "ethlpoll.h"
#include "tms.h"
#include "utils.h"
#include <string.h>



/* return true if MAC address 's' belongs to the CCM multicast group */
/* 01:80:C2:00:00:3y, where 0 <= y < 8 */
#define ETHER_IS_CCM_GROUP(s)	((*(s) == 0x01) && \
				(*(s + 1) == 0x80) && \
				(*(s + 2) == 0xc2) && \
				(*(s + 3) == 0x00) && \
				(*(s + 4) == 0x00) && \
				((*(s + 5) & 0xF8) == 0x30))

/* return true if MAC address 's' belongs to the LTM multicast group */
/* 01:80:C2:00:00:3y, where 8 <= y <= F */
#define ETHER_IS_LTM_GROUP(s)	((*(s) == 0x01) && \
				(*(s + 1) == 0x80) && \
				(*(s + 2) == 0xc2) && \
				(*(s + 3) == 0x00) && \
				(*(s + 4) == 0x00) && \
				((*(s + 5) & 0xF8) == 0x38))

static const char * cannot_set_ltm_parameter="Cannot set LTM parameter";
static const char * cannot_get_ltm_parameter="Cannot get LTM parameter";


//when LTM_sendLTM is set, read rdb parameters fill into config file, then start to send
//$  0 -- success
//$ <0 --- error code
int sendltm_start(Session *pSession)
{
	int err = 0;
	int bok	= 0;
	//char macstr[32];

	ltm_data		*b= &pSession->m_ltm_data;
	action_state	*bs= &pSession->m_ltm_state;
	ltr_data		*r= &pSession->m_ltr_data;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;

	r->ltr_num =0;
	memset(b, 0, sizeof(ltm_data));
	memset(r, 0, sizeof(ltr_data));


	if(pSession->m_mda_state.PeerMode_ready ==0 )
	{
		err =-1;
		sprintf(ls->lasterr, "802.1ag is not started");
		goto lab_err;
	}

	if( bs->Send_started ==0 &&  bs->Send >0)
	{
		unsigned long LTMtransID;
		NTCSLOG_DEBUG("sendltm_start>>>");

		// close previous lmpid
//		ethlctrl_1agcmd(pSession, LTM_RELEASE, l->mpid, 0, 0, 0);
//


		TRY_RDB_GET_P1_UINT(LTM_rmpid, &b->rmpid);

		TRY_RDB_GET_P1_MAC(LTM_destmac, b->destmac);
		err = MEP_RMP_Check(pSession, b->rmpid, b->destmac, ls->lasterr );
		if(err) goto lab_err;

		/// get transid from rdb, set it into OAM

		TRY_RDB_GET_P1_UINT(LTM_LTMtransID, &b->LTMtransID);
		ETH_SET_PARAM(pSession, l->mpid, LMP_NEXTLTMTRANSID, b->LTMtransID, cannot_set_ltm_parameter, ls->lasterr);


		// set param for priority
		// move to rdb_lmp.c
//		TRY_RDB_GET_P1_UINT(LMP_CoS, &l->CoS);
//		ETH_SET_PARAM(pSession, l->mpid, LMP_LTMPRIORITY, (l->CoS&0x7)<<1,cannot_set_ltm_parameter,  ls->lasterr);

		// set param for ttl
		TRY_RDB_GET_P1_UINT(LTM_ttl , &b->ttl);
		ETH_SET_PARAM(pSession, l->mpid, LMP_LTMTTL, b->ttl, cannot_set_ltm_parameter, ls->lasterr);

		//set timeout
		TRY_RDB_GET_P1_UINT(LTM_timeout, &b->timeout);
		ETH_SET_PARAM(pSession, l->mpid, LMP_LTMTIMEOUT, b->timeout, cannot_set_ltm_parameter, ls->lasterr);

		//set flag
		TRY_RDB_GET_P1_UINT(LTM_flag, &b->flag);
		ETH_SET_PARAM(pSession, l->mpid, LMP_LTMFLAG , b->flag, cannot_set_ltm_parameter, ls->lasterr);


		// start LTM
		//TRY_RDB_GET_P1_MAC(LTM_destmac, b->destmac);
		LTMtransID = b->LTMtransID;

		//ETH_1AGTLV(pSession, TLV1AG_SENDERID, &senderid_tlv, ls->lasterr);
		//ETH_1AGCMD(pSession, LTM_TLVENABLE, TLV1AG_SENDERID, 1, 0, 0, ls->lasterr);
//		if(pSession->m_enable_orgspec_tlv)
//		{
//			ETH_1AGTLV(pSession, TLV1AG_ORGSPECIFIC, &pSession->m_orgspec_tlv, ls->lasterr);
//			ETH_1AGCMD(pSession, LTM_TLVENABLE, TLV1AG_ORGSPECIFIC, 1, 0, 0, ls->lasterr);
//		}

//		if( b->rmpid ==0 && ZEROCHK_MACADR(b->destmac))
//		{
//			err = -1;
//			sprintf(ls->lasterr, "Invalid MAC address");
//			goto lab_err;
//		}
		// the b->destmac cannot be
		//1) ZEROCHK_MACADR
		//2) any LTM or CCM group address
		//3) multi-cast or broad-cast address
		bok= ethlctrl_1agcmd(pSession, LTM_ENABLE, l->mpid, b->rmpid,  &LTMtransID,b->destmac);
		if(!bok)
		{
			err = -1;
			sprintf(ls->lasterr, "cannot start LTM test");
			goto lab_err;

		}
		bs->Send_started =1;
		NTCSLOG_DEBUG("sendltm_start<<<");
		goto lab_ok;

	}//if( b->send )

lab_err:

	if(err)
	{
		RDB_SET_P1_2STR(LTM_Status, "Error:  ", ls->lasterr);
		NTCSLOG_INFO("Error:%s", ls->lasterr);
	}
lab_ok:

	if(err ==0)
	{
		RDB_SET_P1_STR(LTM_Status, "Success: LTM is sent");
	}
	bs->Send =0;
	RDB_SET_P1_BOOLEAN(LTM_send, bs->Send);

	return err;
}


// once the sending finish,
int sendltm_end(Session *pSession)
{
	int err=0;
	int bok =0;

	ltm_data		*b= &pSession->m_ltm_data;
	action_state	*bs= &pSession->m_ltm_state;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;
	ltr_data		*r= &pSession->m_ltr_data;

	NTCSLOG_DEBUG("sendltm_end >>>");
	err = bs->status;
	if(bs->Send_started)
	{
		bs->Send_started =0;

		GET_ETH_PARAM(pSession, l->mpid, LMP_NEXTLTMTRANSID,&b->LTMtransID,cannot_get_ltm_parameter, ls->lasterr);
		RDB_SET_P1_INT(LTM_LTMtransID, b->LTMtransID);


		GET_ETH_PARAM(pSession, l->mpid, LMP_LTMFLAG , &b->flag, cannot_get_ltm_parameter, ls->lasterr);
		RDB_SET_P1_INT(LTM_flag, b->flag);



		GET_ETH_PARAM(pSession, l->mpid, LMP_LTRSUNEXPECTED,&l->LTRsUnexpected, cannot_get_ltm_parameter, ls->lasterr);
		RDB_SET_P1_INT(LMP_LTRsUnexpected, l->LTRsUnexpected);


		bok= ethlctrl_1agcmd(pSession, LTM_RELEASE, l->mpid, 0, 0, 0);
		if(!bok)
		{
			err = -1;
			sprintf(ls->lasterr, "cannot release LTM test");
			goto lab_err;
		}

		if(r->ltr_num ==0)
		{		
			RDB_SET_P1_STR(LTM_Status, "Failed: LTM has no reply");
		}
		else
		{
			RDB_SET_P1_STR(LTM_Status, "Success: LTM is completed");
		}
		NTCSLOG_DEBUG("sendltm_end <<<");
		goto lab_ok;
	}

lab_err:
	if(err)
	{
		RDB_SET_P1_2STR(LTM_Status, "Error:  ", ls->lasterr);
	}

lab_ok:
	// TMS 6.81 , LTM is disabled automatically
	//ethlctrl_1agcmd(pSession, LTM_ENABLE, l->mpid, b->rmpid, 0, b->destmac);

	RMP_collect(pSession);

#ifdef _DEBUG
	RDB_SET_P1_STR(DOT1AG_TEST,"ltm");
#endif

	return err;
}


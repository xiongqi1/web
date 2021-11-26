
#include "rdb_comms.h"
#include "ethlctrl.h"
#include "ethlpoll.h"
#include "tms.h"
#include "utils.h"
#include <string.h>

static const char * cannot_set_lbm_parameter="Cannot set LBM parameter";
static const char * cannot_get_lbm_parameter="Cannot get LBM parameter";

//when LBM_sendLBM is set, read rdb parameters fill into config file, then start to send
//$  0 -- success
//$ <0 --- error code
int sendlbm_start(Session *pSession)
{
	int err = 0;
	int bok	= 0;

	lbm_data		*b= &pSession->m_lbm_data;
	action_state	*bs= &pSession->m_lbm_state;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;

	memset(b, 0, sizeof(lbm_data));

	if(pSession->m_mda_state.PeerMode_ready ==0)
	{
		err =-1;
		sprintf(ls->lasterr, "802.1ag is not started");
		goto lab_err;
	}

	if( bs->Send_started ==0 && bs->Send >0)
	{
		NTCSLOG_DEBUG("sendlbm_start>>>");
				// close previous lmpid
//		ethlctrl_1agcmd(pSession, LTM_RELEASE, l->mpid, 0, 0, 0);
//		if(l->mpid) ethlctrl_1agdellmp(pSession, l->mpid);
//		if(b->rmpid)ethlctrl_1agdelrmp(pSession, b->rmpid);



		TRY_RDB_GET_P1_UINT(LBM_rmpid, &b->rmpid);
		TRY_RDB_GET_P1_MAC(LBM_destmac, b->destmac);

		err = MEP_RMP_Check(pSession, b->rmpid, b->destmac, ls->lasterr );
		if(err) goto lab_err;

		// set param for Send
		ETH_SET_PARAM(pSession, l->mpid, LMP_LBMSTOSEND, bs->Send,cannot_set_lbm_parameter, ls->lasterr);

		// set param for nextLBMtransID
		GET_ETH_PARAM(pSession, l->mpid, LMP_NEXTLBMTRANSID,&b->LBMtransID,cannot_get_lbm_parameter,  ls->lasterr);
		RDB_SET_P1_INT(LBM_LBMtransID, b->LBMtransID);

		// set param for timeout
		TRY_RDB_GET_P1_UINT(LBM_timeout, &b->timeout);
		ETH_SET_PARAM(pSession, l->mpid, LMP_LBMTIMEOUT, b->timeout, cannot_set_lbm_parameter, ls->lasterr);

		// set param for priority
		/// move to rdb_lmp
//		TRY_RDB_GET_P1_UINT(LMP_CoS, &l->CoS);
//		ETH_SET_PARAM(pSession, l->mpid, LMP_LBMPRIORITY, (l->CoS&0x7)<<1, cannot_set_lbm_parameter, ls->lasterr);


		TRY_RDB_GET_P1_UINT(LBM_rate, &b->rate);
		ETH_SET_PARAM(pSession, l->mpid, LMP_LBMRATE, b->rate, cannot_set_lbm_parameter, ls->lasterr);

		TRY_RDB_GET_P1_UINT(LBM_TLVDataLen, &b->TLVDataLen);
		if(b->TLVDataLen >=0 && b->TLVDataLen <= 1480)
		{
			if(b->TLVDataLen >0)
			{
				DATA_TLV data;
				memset(&data, 0, sizeof(data));
				data.totalLen = b->TLVDataLen;
				ETH_1AGTLV(pSession, PDUID_LBM, TLV1AG_DATA, &data, ls->lasterr);
				ETH_1AGCMD(pSession, LBM_TLVENABLE, TLV1AG_DATA, 1, 0, 0, ls->lasterr);
			}
			else
			{
				ETH_1AGCMD(pSession, LBM_TLVENABLE, TLV1AG_DATA, 0, 0, 0, ls->lasterr);		
			}
		}
		else
		{
			ETH_1AG_ERROR(TLV1AG_DATA, ls->lasterr);
		}

		// start LBM
		// read rdb
		//TRY_RDB_GET_P1_MAC(LBM_destmac, b->destmac);
		bok= ethlctrl_1agcmd(pSession, LBM_ENABLE, l->mpid, b->rmpid, &b->LBMtransID, b->destmac);
		if(!bok)
		{
			err = -1;
			sprintf(ls->lasterr, "cannot start LBM test");
			goto lab_err;

		}

		pSession->m_lbm_state.Send_started =1;
		NTCSLOG_DEBUG("sendlbm_start<<<");

		goto lab_ok;

	}//if( d->Send >0)

lab_err:

	if(err)
	{
		RDB_SET_P1_2STR(LBM_Status,"Error:  ", ls->lasterr);
		NTCSLOG_INFO("Error: %s", ls->lasterr);
	}

lab_ok:

	if(err ==0)
	{
		RDB_SET_P1_STR(LBM_Status, "Success: LBM is sent");
	}
	bs->Send = 0;
	RDB_SET_P1_INT(LBM_LBMsToSend, bs->Send);
	return err;
}


// once the sending finish,
int sendlbm_end(Session *pSession)
{
	int err=0;
	int bok =0;
	lbm_data		*b= &pSession->m_lbm_data;
	action_state	*bs= &pSession->m_lbm_state;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;

	NTCSLOG_DEBUG("sendlbm_end lmpid = %d >>>", l->mpid);
	err = bs->status;
	if(bs->Send_started )
	{
		bs->Send_started =0;



		GET_ETH_PARAM(pSession, l->mpid, LMP_NEXTLBMTRANSID,&b->LBMtransID, cannot_get_lbm_parameter,ls->lasterr);
		RDB_SET_P1_INT(LBM_LBMtransID, b->LBMtransID);


		GET_ETH_PARAM(pSession, l->mpid, LMP_LBRSINORDER, &l->LBRsInOrder, cannot_get_lbm_parameter,ls->lasterr);
		RDB_SET_P1_INT(LMP_LBRsInOrder, l->LBRsInOrder);

		GET_ETH_PARAM(pSession, l->mpid, LMP_LBRSOUTOFORDER, &l->LBRsOutOfOrder, cannot_get_lbm_parameter, ls->lasterr);
		RDB_SET_P1_INT(LMP_LBRsOutOfOrder, l->LBRsOutOfOrder);

		GET_ETH_PARAM(pSession, l->mpid, LMP_LBRSNOMATCH, &l->LBRnoMatch, cannot_get_lbm_parameter, ls->lasterr);
		RDB_SET_P1_INT(LMP_LBRnoMatch, l->LBRnoMatch);

		GET_ETH_PARAM(pSession, l->mpid, LMP_LBRSTRANSMITTED, &l->LBRsTransmitted, cannot_get_lbm_parameter, ls->lasterr);
		RDB_SET_P1_INT(LMP_LBRsTransmitted, l->LBRsTransmitted);



		if(l->LBRsInOrder ==0 )
		{
			err=-1;
			RDB_SET_P1_STR(LBM_Status, "Failed: LBM has no reply");
		}
		else
		{
			RDB_SET_P1_STR(LBM_Status, "Success: LBM is completed");
		}
		goto lab_ok;

	}

lab_err:
	if(err)
	{
		char msg[64];
		sprintf(msg, "Error: %s", ls->lasterr);
		RDB_SET_P1_STR(LBM_Status, msg);
	}

lab_ok:
	ethlctrl_1agcmd(pSession, LBM_ENABLE, l->mpid, b->rmpid, 0, b->destmac);



#ifdef _DEBUG
	RDB_SET_P1_STR(DOT1AG_TEST,"lbm");
#endif


	NTCSLOG_DEBUG("sendlbm_end <<<");
	return err;
}

// check received packet count,
int count_recv_packet(Session *pSession)
{
	int err=0;
	int bok =0;
	lbm_data		*b= &pSession->m_lbm_data;
	action_state	*bs= &pSession->m_lbm_state;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;

	err = bs->status;
	if(ls->lasterr ==0)
	{

		GET_ETH_PARAM(pSession, l->mpid, LMP_LBMRESPCOUNT , &b->lbmRespCount,cannot_get_lbm_parameter, ls->lasterr);
		return b->lbmRespCount;
	}

lab_err:

	return err;
}

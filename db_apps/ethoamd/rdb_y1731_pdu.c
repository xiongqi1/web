
#include "rdb_comms.h"
#include "ethlctrl.h"
#include "ethlpoll.h"
#include "tms.h"
#include "utils.h"
#include <string.h>
#ifdef NCI_Y1731


static const char * cannot_get_y1731="Cannot get Y.1731 stats";
//static const char * cannot_set_y1731="Cannot set Y.1731 parameters";




//////////////////////////////////////////////
int rdb_set_p1_Y1731_LOSSVAL(const char* name, Session *pSession, LOSSVALUE value)
{
    char buf[64];
    int retValue;
    sprintf(buf, "%u:%u", value.nearEnd, value.farEnd);
    retValue = rdb_set_p1_str(name, pSession->m_node, buf);
    if(retValue)
    {
        return retValue;
    }
    return 0;
}

int rdb_set_p1_Y1731_LOSSVAL_array(const char* name, Session *pSession, LOSSVALUE *value, int len)
{
 	char data[MAX_RDB_VALUE_SIZE];
	int n;
	int l=0;
	int err =0;
	for(n =0; n < len; n++)
	{
		sprintf(&data[l], "%u:%u,", value[n].nearEnd, value[n].farEnd);
		l = strlen(data);
		if(l > (MAX_RDB_VALUE_SIZE-10)) break;
		
	}
	if(l>0 && data[l-1] ==',') data[l-1] = 0;
	err= rdb_set_p1_str(name, pSession->m_node, data);
	 
	if(err == 0)
	{
		return n;
	}
	return err;
}

int rdb_set_p1_OAMTSTAMP(const char* name, Session *pSession, OAMTSTAMP value)
{
    char buf[64];
    int retValue;
    double ms = value.seconds*1000 + (double)value.nanosecs/1000000;
    sprintf(buf, "%.3f", ms);
    retValue = rdb_set_p1_str(name,pSession->m_node, buf);
    if(retValue)
    {
        return retValue;
    }
    return 0;
}

#if 0

/*******************LMM********************************/

//send LMM message
//$  0 -- success
//$ <0 --- error code
int LMM_send(Session *pSession)
{
	int err = 0;
	int bok	= 0;

	lmm_data		*y= &pSession->m_lmm_data;
	action_state	*ys= &pSession->m_lmm_state;

	mda_state		*ms= &pSession->m_mda_state;

	if(ms->PeerMode_ready ==0)
	{
		err =-1;
		sprintf(ms->lasterr, "802.1ag is not started");
		goto lab_err;
	}

	if( ms->Y1731_enable_ready==0)
	{
		err =-1;
		sprintf(ms->lasterr, "Y.1731 is not enabled");
		goto lab_err;
	}

	if( ys->Send_started ==0 && ys->Send >0)
	{

		NTCSLOG_DEBUG("LMM_send>>>");

		TRY_RDB_GET_P1_UINT(Y1731_Lmm_rmpid, &y->rmpid);
		TRY_RDB_GET_P1_MAC(Y1731_Lmm_destmac, y->destmac);

		err = MEP_RMP_Check(pSession, y->rmpid, y->destmac, ms->lasterr );
		if(err) goto lab_err;

		TRY_RDB_GET_P1_UINT(Y1731_Lmm_timeout, &y->timeout);

//		TRY_RDB_GET_P1_UINT(Y1731_Mda_NextTSTSeqID, &y->nexttstseqid);
//		ETH_SET_PARAM(pSession, l->mpid, MDA_NEXTTSTSEQID, y->nexttstseqid, cannot_set_lmm_parameter, ms->lasterr);


		// attention, Send exist in MDA
		//TRY_RDB_GET_P1_INT(Y1731_Lmm_LMMStoSend, &ys->Send);
		//todo
#if 0		
		//ETH_SET_PARAM(pSession, 0, MDA_LMMSTOSEND,  ys->Send, cannot_set_lmm_parameter, ms->lasterr);

		//TRY_RDB_GET_P1_INT(Y1731_Mda_LMMRate, &m->LMMrate);
		//ETH_SET_PARAM(pSession, 0, MDA_LMMRATE, m->LMMrate,cannot_set_lmm_parameter, ms->lasterr);

		/TRY_RDB_GET_P1_INT(Y1731_Lmm_LMMType, &ys->LMMtype);
		if (ys->LMMtype != 1 )
		{
			err =-1;
			sprintf(ms->lasterr, "LMMType must be 1 (singleEnded LMM)");
			goto lab_err;
		}
		ETH_SET_PARAM(pSession, l->mpid, LMP_LMMTYPE ,ys->LMMtype, cannot_set_lmm_parameter, ms->lasterr);

		bok= ethlctrl_1agcmd(pSession, LMM_ENABLE, l->mpid, y->rmpid, 1, y->destmac);
#endif

		if(!bok)
		{
			err = -1;
			sprintf(ms->lasterr, "cannot start LMM test");
			goto lab_err;

		}
		ys->Send_started= 1;

		NTCSLOG_DEBUG("LMM_send><<<");
		goto lab_ok;

	}//if( y->sendLMP )

lab_err:

	if(err)
	{
		RDB_SET_P1_2STR(Y1731_Lmm_Status, "Error:  ", ms->lasterr);
		NTCSLOG_INFO("Error: %s", ms->lasterr);
	}

lab_ok:

	if(err ==0)
	{
		RDB_SET_P1_STR(Y1731_Lmm_Status, "Success: LMM is sent");
	}
	ys->Send =0;
	RDB_SET_P1_INT(Y1731_Lmm_Send, ys->Send);

	return err;
}

// finish sending LMM
int LMM_send_end(Session *pSession)
{
	int err=0;
	//int bok =0;
	//lmm_data		*y= &pSession->m_lmm_data;
	action_state	*ys= &pSession->m_lmm_state;
	
	mda_state		*ms= &pSession->m_mda_state;

	if(ys->Send_started)
	{
		NTCSLOG_DEBUG("LMM_send_end>>>");

		ys->Send_started=0;
#if 0
		GET_ETH_PARAM(pSession, l->mpid, LMP_CURRENT, &y->current, cannot_get_y1731, ms->lasterr);
		rdb_set_Y1731_LOSSVAL(Y1731_Lmm_LMMCurrent, y->current);

		GET_ETH_PARAM(pSession, l->mpid, LMP_ACCUM, &y->accum, cannot_get_y1731, ms->lasterr);
		rdb_set_Y1731_LOSSVAL(Y1731_Lmm_LMMAccum, y->LMMaccum);

		GET_ETH_PARAM(pSession, l->mpid, LMP_RATIO, &y->ratio, cannot_get_y1731, ms->lasterr);
		rdb_set_Y1731_LOSSVAL(Y1731_Lmm_Ratio, y->ratio);
#endif
		NTCSLOG_DEBUG("LMM_send_end<<<");

		goto lab_ok;


lab_err:
		if(err)
		{
			RDB_SET_P1_2STR(Y1731_Lmm_Status, "Error:  ", ms->lasterr);
			NTCSLOG_INFO("failed to stop LMM");
		}
lab_ok:
		if(err ==0)
		{
			RDB_SET_P1_STR(Y1731_Lmm_Status, "Success: LMM is completed");
		}
	}

	return err;
}

#endif

/*********************DMM******************************/

//send DMM message
//$  0 -- success
//$ <0 --- error code
int DMM_send(Session *pSession)
{
	int err = 0;
	int bok	= 0;

	lmp_data		*l= &pSession->m_lmp_data;
	dmm_data		*y= &pSession->m_dmm_data;
	action_state	*ys= &pSession->m_dmm_state;

	//mda_data		*m= &pSession->m_mda_data;
	mda_state		*ms= &pSession->m_mda_state;

	if(ms->PeerMode_ready ==0)
	{
		err =-1;
		sprintf(ms->lasterr, "802.1ag is not started");
		goto lab_err;
	}

	if( ms->Y1731_enable_ready==0)
	{
		err =-1;
		sprintf(ms->lasterr, "Y.1731 is not enabled");
		goto lab_err;
	}

	if( ys->Send_started ==0 && ys->Send >0)
	{
		PDUSETUP setup;
		memset(&setup, 0, sizeof(PDUSETUP));
		NTCSLOG_DEBUG("DMM_send>>>");

		TRY_RDB_GET_P1_UINT(Y1731_Lmp_Cos, &l->YCoS);
		TRY_RDB_GET_P1_UINT(Y1731_Dmm_rmpid, &y->rmpid);
		TRY_RDB_GET_P1_MAC(Y1731_Dmm_destmac, y->destmac);

		err = MEP_RMP_Check(pSession, y->rmpid, y->destmac, ms->lasterr );
		if(err) goto lab_err;

		TRY_RDB_GET_P1_UINT(Y1731_Dmm_timeout, &y->timeout);


		TRY_RDB_GET_P1_UINT(Y1731_Dmm_Rate, &y->Rate);

		TRY_RDB_GET_P1_UINT(Y1731_Dmm_Type, &y->Type);

		TRY_RDB_GET_P1_UINT(Y1731_Dmm_TLVDataLen, &y->TLVDataLen);
		if(y->TLVDataLen >=0 && y->TLVDataLen <= 1446 )
		{
			if(y->TLVDataLen >0)
			{
				DATA_TLV data;
				memset(&data, 0, sizeof(data));
				data.totalLen = y->TLVDataLen;
				ETH_1AGTLV(pSession, PDUID_DMM, TLV1AG_DATA, &data, ms->lasterr);
				ETH_1AGCMD(pSession, DMM_TLVENABLE, TLV1AG_DATA, 1, 0, 0, ms->lasterr);
			}
			else
			{
				ETH_1AGCMD(pSession, DMM_TLVENABLE, TLV1AG_DATA, 0, 0, 0, ms->lasterr);
			}
		}
		else
		{
			ETH_1AG_ERROR(TLV1AG_DATA, ms->lasterr);
		}

		setup.runID = y->runID; /* The application needs to manage the run ID */
		/* since a callback will be made using this run ID */
		if (y->Type)
		{
			setup.pduID = PDUID_DM1; /* A 1DM message is requested */
		}
		else
		{
			setup.pduID = PDUID_DMM; /* A DMM message is requested */
		}
		setup.sendN = ys->Send; /* send  x messages */
		setup.stopped = 0; /* start operations right away */
		setup.interval = y->Rate; /* .5 second rate (500 ms)*/
		setup.rxTimeout = y->timeout; /* time out for responses */
		setup.priDrop = (l->YCoS&0x7)<<1;
		//printf("l->YCoS=%d,l->Cos=%d\n",l->YCoS, l->CoS);
		if(y->rmpid >0)
		{
			setup.rmpID = y->rmpid; /* RMP to send the DMM to */
			setup.pduMode = PDUMODE_RMP; /* Send to RMP that we have heard from before */
		}
		else
		{
			setup.rmpID = 0; /* No RMP */
			setup.pduMode = PDUMODE_OPEN; /* Send to the destination MAC  */
			memcpy(setup.dstMAC, y->destmac, sizeof(setup.dstMAC));
		}	
		
		bok = _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGCMD, PDU_SETUP, l->mpid, &setup);
		if(!bok)
		{
			err = -1;
			if(y->Type)
			{
				sprintf(ms->lasterr, "Cannot send 1DM");
			}
			else
			{
				sprintf(ms->lasterr, "Cannot send DMM");
			}
			goto lab_err;

		}
		y->timeout +=ys->Send*y->Rate;
		ys->Send_started= 1;
		ys->Send =0;

		NTCSLOG_DEBUG("DMM_send<<<");
		goto lab_ok;

	}//if( y->sendLMP )

lab_err:

	if(err)
	{
		RDB_SET_P1_2STR(Y1731_Dmm_Status, "Error:  ", ms->lasterr);
		NTCSLOG_INFO("Error: %s", ms->lasterr);
	}

lab_ok:

	if(err ==0)
	{
		if(y->Type)
		{
			RDB_SET_P1_STR(Y1731_Dmm_Status, "Success: 1DM is sent");
		}
		else
		{
			RDB_SET_P1_STR(Y1731_Dmm_Status, "Success: DMM is sent");
		}
		
	}
	ys->Send =0;
	RDB_SET_P1_INT(Y1731_Dmm_Send, ys->Send);
	return err;
}


// finish sending DMM
int DMM_send_end(Session *pSession)
{
	int err=0;
	int bok =0;
	lmp_data		*l= &pSession->m_lmp_data;
	dmm_data		*y= &pSession->m_dmm_data;
	action_state	*ys= &pSession->m_dmm_state;
	
	mda_state		*ms= &pSession->m_mda_state;


	if(ys->Send_started)
	{
		PDURESULT result;
		NTCSLOG_DEBUG("DMM_send_end>>>");

		ys->Send_started=0;
		if(y->Type == 0) // DMM
		{
			bok= _ethLinPOLL(pSession->m_node, ETHLPOLL_1AGPDU, l->mpid, y->runID, y->rmpid, &result);
			if(!bok) 
			{ 
				err = -1;
				build_err_msg(ms->lasterr, cannot_get_y1731, ETHLPOLL_1AGPDU, "ETHLPOLL_DMM");
				goto lab_err;	
			}
			y->Dly = result.dlyResult.dly.val;
			y->DlyAvg = result.dlyResult.dly.avg;
			y->DlyMin = result.dlyResult.dly.min;
			y->DlyMax = result.dlyResult.dly.max;
			y->Var = result.dlyResult.variation.val;
			y->VarAvg = result.dlyResult.variation.avg;
			y->VarMin = result.dlyResult.variation.min;
			y->VarMax = result.dlyResult.variation.max;
			y->Count	 = result.dlyResult.rxCount;
			
			
			rdb_set_p1_OAMTSTAMP(Y1731_Dmm_Dly, pSession, y->Dly);
			rdb_set_p1_OAMTSTAMP(Y1731_Dmm_DlyAvg, pSession, y->DlyAvg );
			rdb_set_p1_OAMTSTAMP(Y1731_Dmm_DlyMin, pSession, y->DlyMin);
			rdb_set_p1_OAMTSTAMP(Y1731_Dmm_DlyMax, pSession, y->DlyMax);
			rdb_set_p1_OAMTSTAMP(Y1731_Dmm_Var, 		pSession, y->Var);
			rdb_set_p1_OAMTSTAMP(Y1731_Dmm_VarAvg,	pSession, y->VarAvg );
			rdb_set_p1_OAMTSTAMP(Y1731_Dmm_VarMin, 	pSession, y->VarMin);
			rdb_set_p1_OAMTSTAMP(Y1731_Dmm_VarMax, 	pSession, y->VarMax);
			RDB_SET_P1_INT(Y1731_Dmm_Count, 	y->Count);				
			bok = _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGCMD, PDU_COMMAND,  l->mpid, y->runID, PDUCMD_DELETE);
			
			
			if(err || ys->Event.m_has_error)
			{
				RDB_SET_P1_STR(Y1731_Dmm_Status, "Error: in sending DMM");
			}
			else if(err ==0 )
			{
				
				if(y->Count == 0)
				{
					//no receiving
					RDB_SET_P1_STR(Y1731_Dmm_Status, "Failed: DMM has no response");
				}
				else
				{
					RDB_SET_P1_STR(Y1731_Dmm_Status, "Success: DMM is completed");
				}
			}
		}
		else // 1DM
		{
			bok = _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGCMD, PDU_COMMAND,  l->mpid, y->runID, PDUCMD_DELETE);
			

			if(ys->Event.m_has_error)
			{
				RDB_SET_P1_STR(Y1731_Dmm_Status, "Error: in sending 1DM");
			}
			else if(err ==0 )
			{
				RDB_SET_P1_STR(Y1731_Dmm_Status, "Success: 1DM is completed");
			}
		}
		NTCSLOG_DEBUG("DMM_send_end<<<");

	}
lab_err:
	if(err)
	{
		RDB_SET_P1_2STR(Y1731_Dmm_Status, "Error:  ", ms->lasterr);
		NTCSLOG_INFO("failed to stop DMM");
	}
	return err;
}

/**********************SLM*****************************/
//send SLM message
//$  0 -- success
//$ <0 --- error code
int SLM_send(Session *pSession)
{
	int err = 0;
	int bok	= 0;

	lmp_data		*l= &pSession->m_lmp_data;
	slm_data		*y= &pSession->m_slm_data;
	action_state	*ys= &pSession->m_slm_state;

	mda_state		*ms= &pSession->m_mda_state;

	if(ms->PeerMode_ready ==0)
	{
		err =-1;
		sprintf(ms->lasterr, "802.1ag is not started");
		goto lab_err;
	}

	if( ms->Y1731_enable_ready==0)
	{
		err =-1;
		sprintf(ms->lasterr, "Y.1731 is not enabled");
		goto lab_err;
	}

	if( ys->Send_started ==0 && ys->Send >0)
	{
		PDUSETUP setup;
		int i;
		memset(&setup, 0, sizeof(PDUSETUP));
		NTCSLOG_DEBUG("SLM_send>>>");

		TRY_RDB_GET_P1_UINT(Y1731_Lmp_Cos, &l->YCoS);
		TRY_RDB_GET_P1_UINT(Y1731_Slm_rmpid, &y->rmpid);
		TRY_RDB_GET_P1_MAC(Y1731_Slm_destmac, y->destmac);


		err = MEP_RMP_Check(pSession, y->rmpid, y->destmac, ms->lasterr );
		if(err) goto lab_err;

		TRY_RDB_GET_P1_UINT(Y1731_Slm_timeout, &y->timeout);


		TRY_RDB_GET_P1_UINT(Y1731_Slm_Rate, &y->Rate);
		

		TRY_RDB_GET_P1_UINT(Y1731_Slm_TLVDataLen, &y->TLVDataLen);
		if(y->TLVDataLen >= 0 && y->TLVDataLen <= 1460)
		{
			if(y->TLVDataLen >0)
			{
				DATA_TLV data;
				memset(&data, 0, sizeof(data));
				data.totalLen = y->TLVDataLen;
				ETH_1AGTLV(pSession, PDUID_SLM, TLV1AG_DATA, &data, ms->lasterr);
				ETH_1AGCMD(pSession, SLM_TLVENABLE, TLV1AG_DATA, 1, 0, 0, ms->lasterr);
			}
			else
			{
				ETH_1AGCMD(pSession, SLM_TLVENABLE, TLV1AG_DATA, 0, 0, 0, ms->lasterr);
			}
		}
		else
		{
			ETH_1AG_ERROR(TLV1AG_DATA, ms->lasterr);
		}

		setup.runID = y->runID; /* The application needs to manage the run ID */
		/* since a callback will be made using this run ID */
		
		setup.pduID = PDUID_SLM; /* A SLM message is requested */
		
		
		setup.sendN = ys->Send; /* send  x messages */
		setup.stopped = 0; /* start operations right away */
		setup.interval = y->Rate; /* .5 second rate (500 ms)*/
		setup.rxTimeout = y->timeout; /* time out for responses */
		setup.priDrop = (l->YCoS&0x7)<<1;
		if(y->rmpid >0)
		{
			setup.rmpID = y->rmpid; /* RMP to send the SLM to */
			setup.pduMode = PDUMODE_RMP; /* Send to RMP that we have heard from before */
		}
		else
		{
			setup.rmpID = 0; /* No RMP */
			setup.pduMode = PDUMODE_OPEN; /* Send to the destination MAC  */
			memcpy(setup.dstMAC, y->destmac, sizeof(setup.dstMAC));
		}	
		
		err =rdb_get_index_list(Y1731_Slm_TestID, pSession->m_node,  (int*)y->testID, MAX_MULTI_SLM_SESSION);
		if (err <= 0)
		{
			err =-1;
			strcpy(ms->lasterr, "Invalid SLM TestID");
			goto lab_err;
		}
		
		y->testID_num = err;
		err =0;
		for (i =0; i <y->testID_num ; i++)
		{
			setup.runID = y->runID+i;
			setup.slmTestID = y->testID[i];
			bok = _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGCMD, PDU_SETUP, l->mpid, &setup);
			if(!bok)
			{
				err = -1;
				sprintf(ms->lasterr, "Cannot send SLM");
				goto lab_err;

			}
			y->state[i] = SLM_STATE_SENT;
			
		}
		y->timeout +=(ys->Send+1)*y->Rate*y->testID_num;
		ys->Send_started= 1;
		ys->Send =0;
		
		NTCSLOG_DEBUG("SLM_send<<<");
		goto lab_ok;
	
	}//if( y->sendLMP )

lab_err:

	if(err)
	{
		RDB_SET_P1_2STR(Y1731_Slm_Status, "Error:  ", ms->lasterr);
		NTCSLOG_INFO("Error: %s", ms->lasterr);
	}

lab_ok:

	if(err ==0)
	{
		RDB_SET_P1_STR(Y1731_Slm_Status, "Success: SLM is sent");
		
	}
	ys->Send =0;
	RDB_SET_P1_INT(Y1731_Slm_Send, ys->Send);
	return err;
}


// finish sending SLM
int SLM_send_end(Session *pSession)
{
	int err=0;
	int bok =0;
	int i;
	int count =0;
	
	lmp_data		*l= &pSession->m_lmp_data;
	slm_data		*y= &pSession->m_slm_data;
	action_state	*ys= &pSession->m_slm_state;
	
	mda_state		*ms= &pSession->m_mda_state;


	if(ys->Send_started)
	{
		PDURESULT result;
		NTCSLOG_DEBUG("SLM_send_end>>>");

		ys->Send_started=0;
		for (i =0; i<y->testID_num; i++ )
		{
		
			bok= _ethLinPOLL(pSession->m_node, ETHLPOLL_1AGPDU, l->mpid, y->runID +i, y->rmpid, &result);
			if(!bok) 
			{ 
				err = -1;
				build_err_msg(ms->lasterr, cannot_get_y1731, ETHLPOLL_1AGPDU, "ETHLPOLL_SLM");
				goto lab_err;	
			}
			bok = _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGCMD, PDU_COMMAND,  l->mpid, y->runID+i, PDUCMD_DELETE);
	
			count +=  result.lossResult.rxCount;
			y->Count[i] = result.lossResult.rxCount;
			y->curr[i]	 = result.lossResult.curr;
			y->accum[i] = result.lossResult.accum;
			y->ratio[i] = result.lossResult.ratio;
		}
		rdb_set_p1_Y1731_LOSSVAL_array(Y1731_Slm_Curr, pSession, y->curr, y->testID_num);
		rdb_set_p1_Y1731_LOSSVAL_array(Y1731_Slm_Accum, pSession, y->accum, y->testID_num);
		rdb_set_p1_Y1731_LOSSVAL_array(Y1731_Slm_Ratio, pSession, y->ratio, y->testID_num);
		rdb_set_p1_uint_array(Y1731_Slm_Count, pSession->m_node, y->Count, y->testID_num);				
	

		
		
		
		if(err || ys->Event.m_has_error)
		{
			RDB_SET_P1_STR(Y1731_Slm_Status, "Error: in sending SLM");
		}
		else if(err ==0 )
		{
			
			if(count == 0)
			{
				//no receiving
				RDB_SET_P1_STR(Y1731_Slm_Status, "Failed: SLM has no response");
			}
			else
			{
				RDB_SET_P1_STR(Y1731_Slm_Status, "Success: SLM is completed");
			}
		}

		NTCSLOG_DEBUG("SLM_send_end<<<");

	}
lab_err:
	if(err)
	{
		RDB_SET_P1_2STR(Y1731_Slm_Status, "Error:  ", ms->lasterr);
		NTCSLOG_INFO("failed to stop SLM");
	}
	return err;
}





#endif // NCI_Y1731


#include "rdb_comms.h"
#include "ethlctrl.h"
#include "ethlpoll.h"
#include "tms.h"
#include "utils.h"
#include <string.h>

static const char * cannot_change_mep="Cannot change MEP to";
static const char * cannot_set_priority_parameter="Cannot set Cos/Priority";
static const char * cannot_get_mep_stats="Cannot get MEP stats";
static const char * cannot_get_y1731="Cannot get Y.1731 stats";

//$  0 -- success
//$ <0 --- error code
int valid_lmp(Session *pSession)
{
	lmp_data		*l= &pSession->m_lmp_data;
	int direction = l->direction;
	//1) l->mpid must be between 1~8191
	if ((l->mpid< 1) || (l->mpid > 8191))
		return(-1);

	// this is mistake in document to Erison about direction, I am going correct it
	if (direction>1 )
	{
		direction=-1;
	}

	if ((direction == -1) || (direction == 1)) {

		/* if it's a 1AG-MEP, the MA has to be set
		 */
		if (!pSession->m_mda_state.Y1731_MDA_enable && !pSession->m_mda_data.MaLength)
			return(-1);
	}
	else if (direction != 0) {

		return(-1);
	}
	return 0;
}
//enable LMP
//$  0 -- success
//$ <0 --- error code
int LMP_enable(Session *pSession)
{
	int err = 0;
	int bok	= 0;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;
#ifdef NCI_Y1731
	mda_state		*ms= &pSession->m_mda_state;
#endif
	if(pSession->m_mda_state.PeerMode_ready ==0)
	{
		err =-1;
		sprintf(ls->lasterr, "802.1ag is not started");
		goto lab_err;
	}

	if( ls->LMP_enable_ready == 0)
	{
		TRY_RDB_GET_P1_UINT(LMP_mpid, &l->mpid);
		TRY_RDB_GET_P1_INT(LMP_direction, &l->direction);
		TRY_RDB_GET_P1_UINT(LMP_port, &l->port);
		TRY_RDB_GET_P1_UINT(LMP_vid, &l->vid);
		TRY_RDB_GET_P1_UINT(LMP_vidtype, &l->vidtype);
		TRY_RDB_GET_P1_UINT(LMP_CoS, &l->CoS);
		TRY_RDB_GET_P1_UINT(Y1731_Lmp_Cos, &l->YCoS);

		err = RDB_GET_P1_MAC(LMP_macAdr, l->macAdr);
// if the LMP_macAdr is invalid or all zero, get mac from local interface
		if(err || ZEROCHK_MACADR(l->macAdr))
		{
#ifdef _AVCID_MAC
			sprintf(ls->lasterr, "Invalid Local MAC on LMP %d", l->mpid);
			goto lab_err;
#else
			memcpy(l->macAdr, pSession->m_if_mac, sizeof(pSession->m_if_mac));
			RDB_SET_P1_MAC(LMP_macAdr, l->macAdr);
#endif
		}

		NTCSLOG_DEBUG("LMP_enable(%d) >>>",l->mpid);
		err= valid_lmp(pSession);
		if(err)
		{
			sprintf(ls->lasterr, "Invalid LMP: lmpid=%d or direction=%d", l->mpid, l->direction);
			goto lab_err;
		}

		err = setupLMP(pSession, l->mpid, l->direction>1?-1:l->direction, l->port, l->vid ,l->vidtype, l->macAdr);
		if(err)
		{
			sprintf(ls->lasterr, "cannot setup LMP end point %d", l->mpid);
			goto lab_err;
		}
		
		ls->LMP_enable_ready =1;
		

		/// for LBM/LTM
		// LMP_CCMPRIORITYDROP is vlan priority bit,COs need left shift: 1->2, 2->4,7 ->0xE
		// the following function could fail.
		ETH_SET_PARAM(pSession, l->mpid, LMP_LBMPRIORITYDROP, (l->CoS&0x7)<<1, cannot_set_priority_parameter, ls->lasterr);
		ETH_SET_PARAM(pSession, l->mpid, LMP_LTMPRIORITYDROP, (l->CoS&0x7)<<1,cannot_set_priority_parameter,  ls->lasterr);
#ifdef NCI_Y1731
		if (ms->Y1731_MDA_enable)
		{
			/// for Y1731
			//ETH_SET_PARAM(pSession, l->mpid, LMP_AISPRIORITYDROP,(l->YCoS&0x7)<<1, cannot_set_priority_parameter, ls->lasterr);

		}
#endif

		NTCSLOG_DEBUG("LMP_enable(%d) <<<",l->mpid);
		goto lab_ok;

	}//if( l->sendLMP )

lab_err:

	if(err)
	{
		RDB_SET_P1_2STR(LMP_Status, "Error:  ", ls->lasterr);
		NTCSLOG_INFO("failed to enable LMP");
	}

lab_ok:
	if(err ==0)
	{
		RDB_SET_P1_STR(LMP_Status, "Success: LMP is enabled");
	}

	return err;
}

// disable LMP
int LMP_disable(Session *pSession)
{
	int err=0;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;

	if(pSession->m_mda_state.PeerMode_ready ==0)
	{
		err =-1;
		sprintf(ls->lasterr, "802.1ag is not started");
		goto lab_err;
	}

	if(ls->LMP_enable_ready)
	{
		CCM_disable(pSession);
		
		MEP_disable(pSession);

		NTCSLOG_DEBUG("LMP_disable(%d) >>>",l->mpid);

		ethlctrl_1agdellmp(pSession, l->mpid);
		ls->LMP_enable_ready =0;

		RDB_SET_P1_STR(LMP_Status, "Success: LMP is disabled");

		NTCSLOG_DEBUG("LMP_disable(%d) <<<",l->mpid);

		l->mpid =0;
	}

lab_err:

	return err;
}

// check whether lmp config changed
// >0 -- changed
// 0 -- not changed
int check_lmp_config(Session *pSession)
{
	int err;
	unsigned int tmp;
	const char *rdb_name=0;
	MACADR	macAdr;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;
	if(ls->Reload_config)
	{
		ls->Reload_config=0;
		NTCSLOG_DEBUG("MEPActive force to reload LMP Config");
		return 1;
	}


	RDB_CMP_P1_UINT(LMP_mpid, tmp, l->mpid);
	RDB_CMP_P1_UINT(LMP_direction, tmp, l->direction);
	RDB_CMP_P1_UINT(LMP_port, tmp, l->port);
	RDB_CMP_P1_UINT(LMP_vid, tmp, l->vid);
	RDB_CMP_P1_UINT(LMP_vidtype, tmp, l->vidtype);
	RDB_CMP_P1_UINT(LMP_CoS, tmp, l->CoS);
#ifdef NCI_Y1731	
	RDB_CMP_P1_UINT(Y1731_Lmp_Cos, tmp, l->YCoS);
#endif	
	RDB_CMP_P1_MAC(LMP_macAdr, macAdr, l->macAdr);
	NTCSLOG_DEBUG("LMP Config no change");
	return 0;

lab_value_changed:
	NTCSLOG_DEBUG("LMP Config changed, %s =%d", rdb_name, tmp);
	return 1;
lab_err:
	return 0;
}

//enable MEP
//$  0 -- success
//$ <0 --- error code
int MEP_enable(Session *pSession)
{
	int err = 0;
	int bok	= 0;

	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;

	if(pSession->m_mda_state.PeerMode_ready ==0)
	{
		err =-1;
		sprintf(ls->lasterr, "802.1ag is not started");
		goto lab_err;
	}

	if( ls->MEP_active_ready ==0)
	{

		NTCSLOG_DEBUG("MEP_enable(%d) >>>", l->mpid);

		err = LMP_enable(pSession);
		if(err)
		{
			sprintf(ls->lasterr, "cannot setup LMP end point %d", l->mpid);
			goto lab_err;
		}

		ETH_SET_PARAM(pSession, l->mpid, LMP_ACTIVE, 1, cannot_change_mep,ls->lasterr);


		ls->MEP_active_ready =1;
		//TRY_RDB_SET_P1_BOOLEAN(LMP_MEPactive, ls->MEP_active_ready);
		NTCSLOG_DEBUG("MEP_enable(%d) <<<", l->mpid);
		goto lab_ok;

	}//if( l->sendLMP )


lab_err:

	if(err)
	{
		RDB_SET_P1_2STR(LMP_Status, "Error:  ", ls->lasterr);
		NTCSLOG_INFO("Error: %s", ls->lasterr);
	}

lab_ok:

	if(err ==0)
	{
		RDB_SET_P1_STR(LMP_Status, "Success: MEP is active");
	}

	return err;
}

// disable MEP
//$  0 -- success
//$ <0 --- error code
int MEP_disable(Session *pSession)
{
	int err=0;
	int bok =0;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;

	if(pSession->m_mda_state.PeerMode_ready ==0)
	{
		err =-1;
		sprintf(ls->lasterr, "802.1ag is not started");
		goto lab_err;
	}

	if(ls->MEP_active_ready)
	{
		NTCSLOG_DEBUG("MEP_disable(%d) >>>", l->mpid);

		ls->MEP_active_ready =0;
		ETH_SET_PARAM(pSession, l->mpid, LMP_ACTIVE, 0, cannot_change_mep, ls->lasterr);

		//TRY_RDB_SET_P1_BOOLEAN(LMP_MEPactive, ls->MEP_active_ready);

		NTCSLOG_DEBUG("MEP_disable(%d) <<<",l->mpid);

		goto lab_ok;
	}

lab_err:
	if(err)
	{
		RDB_SET_P1_2STR(LMP_Status, "Error:  ", ls->lasterr);
		NTCSLOG_INFO("failed to disable MEP");
	}
lab_ok:

	//LMP_sync(pSession);


	if(err ==0)
	{
		RDB_SET_P1_STR(LMP_Status, "Success: MEP is inactive");
	}

	return err;
}
//auto adjust MEP state
//$  1 -- no change
//$  0 -- success changed
//$ <0 --- error code
int MEP_auto_start(Session *pSession)
{
	if (check_lmp_config(pSession))
	{
		LMP_disable(pSession);
	}
	if(pSession->m_lmp_state.MEPactive != pSession->m_lmp_state.MEP_active_ready)
	{
		if(pSession->m_lmp_state.MEPactive)
		{
			return MEP_enable(pSession);
		}
		else
		{
			return MEP_disable(pSession);
		}
	}

	return 1;
}

//enable MEP check RMP
//$  0 -- success
//$ <0 --- error code
int MEP_RMP_Check(Session *pSession, unsigned int rmpid, MACADR	destmac, char *lasterr)
{
	int err ;
	lmp_data		*l= &pSession->m_lmp_data;

	// setup LMP
	if(check_lmp_config(pSession))
	{
		LMP_disable(pSession); // it also disable MEP
	}
	err = MEP_enable(pSession);
	if(err)
	{
		sprintf(lasterr, "cannot setup LMP end point %d", l->mpid);
		l->mpid =0;
		goto lab_err;

	}
	// setup RMP
	if(rmpid >0 )
	{
		//err = setupRMP(pSession->m_node, b->rmpid);
		int objectid = RMP_find(pSession, rmpid);
		if(objectid < 0)
		{
			err = -1;
			sprintf(lasterr, "RMP %d object has not setup yet", rmpid);
			goto lab_err;
		}

		err = RMP_enable(pSession, objectid);
		if(err)
		{
			sprintf(lasterr, "cannot setup RMP end point %d", rmpid);
			goto lab_err;

		}
	}
	if( rmpid ==0 && ZEROCHK_MACADR(destmac))
	{
		err = -1;
		sprintf(lasterr, "Invalid MAC address");
		goto lab_err;
	}
lab_err:

	return err;
}


static const char * cannot_install_mda_parameter="Cannot install mda parameter";

// check whether any mda parameter, if changed ,reload
// >0 -- changed
// 0 -- not changed
int install_extra_mda_config(Session *pSession)
{
	int bok	= 0;
	int err = 0;
	unsigned int tmp;
	const char *rdb_name=0;
	mda_data		*m= &pSession->m_mda_data;
	mda_state		*ms= &pSession->m_mda_state;

	if(ms->PeerMode_ready)
	{
		RDB_CMP_P1_UINT(MDA_lowestAlarmPri, tmp, m->lowestAlarmPri);
		RDB_CMP_P1_UINT(MDA_fngAlarmTime, tmp, m->fngAlarmTime);
		RDB_CMP_P1_UINT(MDA_fngResetTime, tmp, m->fngResetTime);

		NTCSLOG_DEBUG("MDA extra config no change");
	}
	return 0;

lab_value_changed:

	TRY_RDB_GET_P1_UINT(MDA_lowestAlarmPri, &m->lowestAlarmPri);
	ETH_SET_PARAM(pSession, 0, MDA_LOWESTALARMPRI, m->lowestAlarmPri, cannot_install_mda_parameter, ms->lasterr);

	TRY_RDB_GET_P1_UINT(MDA_fngAlarmTime, &m->fngAlarmTime);
	ETH_SET_PARAM(pSession, 0, MDA_FNGALARMTIME, m->fngAlarmTime, cannot_install_mda_parameter, ms->lasterr);

	TRY_RDB_GET_P1_UINT(MDA_fngResetTime, &m->fngResetTime);
	ETH_SET_PARAM(pSession, 0, MDA_FNGRESETTIME, m->fngResetTime, cannot_install_mda_parameter, ms->lasterr);


	//NTCSLOG_DEBUG("MDA Parameter changed, %s", rdb_name);
	return 1;

lab_err:
	return 0;
}



//collect all stats on this MEP
//$  0 -- success
//$ <0 --- error code
int MEP_collect(Session *pSession)
{
	int err = 0;
	int bok	= 0;

	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;

	mda_data		*m= &pSession->m_mda_data;
	mda_state		*ms= &pSession->m_mda_state;


	if( ls->MEP_active_ready)
	{

		NTCSLOG_DEBUG("MEP_collect>>>");

		GET_ETH_PARAM(pSession, 0, MDA_ERRORCCMDEFECT, &m->errorCCMdefect, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(MDA_errorCCMdefect, m->errorCCMdefect);

		GET_ETH_PARAM(pSession, 0, MDA_XCONNCCMDEFECT, &m->xconCCMdefect, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(MDA_xconCCMdefect, m->xconCCMdefect);

		GET_ETH_PARAM(pSession, 0, MDA_CCMSEQUENCEERRORS, &m->CCMsequenceErrors, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(MDA_CCMsequenceErrors, m->CCMsequenceErrors);

		GET_ETH_PARAM(pSession, 0, MDA_SOMERMEPCCMDEFECT, &m->someRMEPCCMdefect, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(MDA_someRMEPCCMdefect, m->someRMEPCCMdefect);

		GET_ETH_PARAM(pSession, 0, MDA_SOMEMACSTATUSDEFECT, &m->someMACstatusDefect, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(MDA_someMACstatusDefect, m->someMACstatusDefect);

		GET_ETH_PARAM(pSession, 0, MDA_SOMERDIDEFECT, &m->someRDIdefect, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(MDA_someRDIdefect, m->someRDIdefect);

		GET_ETH_PARAM(pSession, 0, MDA_HIGHESTDEFECT, &m->highestDefect, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(MDA_highestDefect, m->highestDefect);

		GET_ETH_PARAM(pSession, 0, MDA_FNGSTATE, &m->fngState, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(MDA_fngState, m->fngState);

#ifdef NCI_Y1731	
		if(ms->Y1731_enable_ready)
		{ 
			GET_ETH_PARAM(pSession, 0, MDA_ALMSUPPRESSED, &m->almSuppressed, cannot_get_y1731, ms->lasterr);
			RDB_SET_P1_INT(Y1731_Mda_ALMSuppressed, m->almSuppressed);
		}
#endif
	

		GET_ETH_PARAM(pSession, l->mpid, LMP_CCMSENT, &l->CCMsent, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(LMP_CCIsentCCMs, l->CCMsent);

		GET_ETH_PARAM(pSession, l->mpid, LMP_XCONNCCMDEFECT, &l->xconnCCMdefect, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(LMP_xconnCCMdefect, l->xconnCCMdefect);

		GET_ETH_PARAM(pSession, l->mpid, LMP_ERRORCCMDEFECT, &l->errorCCMdefect, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(LMP_errorCCMdefect, l->errorCCMdefect);

		GET_ETH_PARAM(pSession, l->mpid, LMP_CCMSEQUENCEERRORS, &m->CCMsequenceErrors, cannot_get_mep_stats, ms->lasterr);
		RDB_SET_P1_INT(LMP_CCMsequenceErrors, m->CCMsequenceErrors);

		GET_ETH_PARAM(pSession, l->mpid, LMP_TXCOUNTER, &l->TxCounter, cannot_get_mep_stats,ms->lasterr);
		RDB_SET_P1_INT(LMP_TxCounter, l->TxCounter);
		
		GET_ETH_PARAM(pSession, l->mpid, LMP_RXCOUNTER, &l->RxCounter, cannot_get_mep_stats,ms->lasterr);
		RDB_SET_P1_INT(LMP_RxCounter, l->RxCounter);


		NTCSLOG_DEBUG("MEP_collect<<<");
		goto lab_ok;

	}//if( ls->MEP_active_ready)

lab_err:

	if(err)
	{
		RDB_SET_P1_2STR(LMP_Status, "Error:  ", ms->lasterr);
		NTCSLOG_INFO("Error: %s", ms->lasterr);
	}

lab_ok:


	return err;
}

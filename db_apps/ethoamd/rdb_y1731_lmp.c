
#include "rdb_comms.h"
#include "ethlctrl.h"
#include "ethlpoll.h"
#include "tms.h"
#include "utils.h"
#include <string.h>
#ifdef NCI_Y1731

//static const char * cannot_set_ais_parameter="Cannot set AIS parameter";


//static const char * cannot_get_y1731="Cannot get Y.1731 stats";

/*--------------AIS-------------------------*/
#if 0
//enable AIS
//$  0 -- success
//$ <0 --- error code
int AIS_enable(Session *pSession)
{
	int err = 0;
	int bok	= 0;

	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;

	mda_data		*m= &pSession->m_mda_data;
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
	TRY_RDB_GET_P1_BOOLEAN(Y1731_Lmp_AISAuto, &ls->AISauto);
	TRY_RDB_GET_P1_BOOLEAN(Y1731_Lmp_AISForced, &ls->AISforced);

	if( ls->AIS_force_ready  != ls->AISforced || ls->AIS_auto_ready != ls->AISauto)
	{

		NTCSLOG_DEBUG("AIS_enable>>>");

		err = MEP_enable(pSession);
		if(err)
		{
			sprintf(ls->lasterr, "cannot setup LMP end point %d", l->mpid);
			goto lab_err;
		}

		TRY_RDB_GET_P1_INT(Y1731_Mda_AISInterval, &m->AISinterval);
		ETH_SET_PARAM(pSession, 0, MDA_AISINTERVAL, m->AISinterval,cannot_set_ais_parameter, ls->lasterr);

		TRY_RDB_GET_P1_INT(Y1731_Lmp_AISMultiCast, &l->AISmulticast);
		ETH_SET_PARAM(pSession, l->mpid, LMP_AISMULTICAST, l->AISmulticast,cannot_set_ais_parameter, ls->lasterr);

		TRY_RDB_GET_P1_MAC(Y1731_Lmp_AISUniCastMac, l->AISunicastMAC);
		ETH_SET_PARAM(pSession, l->mpid, LMP_AISUNICASTMAC, (UNITYPE)l->AISunicastMAC,cannot_set_ais_parameter, ls->lasterr);


	
		/// move to rdb_lmp.c
		///TRY_RDB_GET_P1_INT(Y1731_Lmp_AISPriority, &l->AISpriority);
		///ETH_SET_PARAM(pSession, l->mpid, LMP_AISPRIORITY,l->AISpriority,cannot_set_ais_parameter, ls->lasterr);

		/// no need to set, driver get it from mdlevel
		///TRY_RDB_GET_P1_INT(Y1731_Lmp_AISClientlvl, &l->AISclientLvl);
		///ETH_SET_PARAM(pSession,  l->mpid, LMP_AISCLIENTLVL,l->AISclientLvl,cannot_set_ais_parameter, ls->lasterr);

		/// no need to set, driver get it from vid
		///TRY_RDB_GET_P1_INT(Y1731_Lmp_AISClientvid, &l->AISclientVid);
		///ETH_SET_PARAM(pSession,  l->mpid, LMP_AISCLIENTVID,l->AISclientVid,cannot_set_ais_parameter, ls->lasterr);

		//ls->AISauto and ls->AISforced are watched
		ETH_SET_PARAM(pSession,  l->mpid, LMP_AISAUTO,ls->AISauto,cannot_set_ais_parameter, ls->lasterr);

		ETH_SET_PARAM(pSession, l->mpid, LMP_AISFORCED, ls->AISforced, cannot_set_ais_parameter,ls->lasterr);

		ls->AIS_force_ready = ls->AISforced;
		ls->AIS_auto_ready	= ls->AISauto;

		NTCSLOG_DEBUG("AIS_enable<<<");
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
		RDB_SET_P1_STR(LMP_Status, "Success: AIS is enabled");
	}

	return err;
}
// disable AIS
int AIS_disable(Session *pSession)
{
	int err=0;
	int bok =0;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;

	//mda_data		*m= &pSession->m_mda_data;

	if(ls->AIS_force_ready || ls->AIS_auto_ready)
	{
		NTCSLOG_DEBUG("AIS_disable >>>");


		ETH_SET_PARAM(pSession, l->mpid, LMP_AISFORCED, ls->AISforced, cannot_set_ais_parameter,ls->lasterr);
		ETH_SET_PARAM(pSession, l->mpid, LMP_AISAUTO, ls->AISauto, cannot_set_ais_parameter,ls->lasterr);

		ls->AIS_force_ready= ls->AISforced;
		ls->AIS_auto_ready = ls->AISauto;

		NTCSLOG_DEBUG("AIS_disable <<<");

		goto lab_ok;
	}

lab_err:
	if(err)
	{
		RDB_SET_P1_2STR(LMP_Status, "Error:  ", ls->lasterr);
		NTCSLOG_INFO("failed to disable AIS");
	}
lab_ok:



	if(err ==0)
	{
		RDB_SET_P1_STR(LMP_Status, "Success: AIS is disabled");
	}

	return err;
}

// auto adjust AIS state
//$  0 -- success
//$ <0 --- error code
int AIS_auto_start(Session *pSession)
{
	int force_state_changed =pSession->m_lmp_state.AISforced - pSession->m_lmp_state.AIS_force_ready;
	int enable_state_changed =pSession->m_lmp_state.AISauto - pSession->m_lmp_state.AIS_auto_ready;


	if(force_state_changed || enable_state_changed )
	{

		if(force_state_changed >0 || enable_state_changed >0)
		{
			return AIS_enable(pSession);
		}
		else
		{
			return AIS_disable(pSession);
		}
	}
	return 0;
}

#endif

#if 0
// collect Y1731 parameter
//$  0 -- success
//$ <0 --- error code
int Y1731_Collect(Session* pSession)
{
	int err=0;
	int bok =0;
	lmp_state		*ls= &pSession->m_lmp_state;

	mda_data		*m= &pSession->m_mda_data;
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

	if( ls->LMP_enable_ready)
	{
		NTCSLOG_DEBUG("Y1731_collect>>>");

		GET_ETH_PARAM(pSession, 0, MDA_ALMSUPPRESSED, &m->almSuppressed, cannot_get_y1731, ms->lasterr);
		RDB_SET_P1_INT(Y1731_Mda_ALMSuppressed, m->almSuppressed);

		//GET_ETH_PARAM(pSession, 0, MDA_SOMERDIDEFECT, &m->someRDIdefect, cannot_get_y1731, ms->lasterr);
		//RDB_SET_P1_INT(A_Y1731_Mda_someRDIdefect, m->someRDIdefect);


		NTCSLOG_DEBUG("Y1731_collect<<<");

	}
lab_err:

	if(err)
	{
		RDB_SET_P1_2STR(LMP_Status, "Error:  ", ms->lasterr);
		NTCSLOG_INFO("Error: %s", ms->lasterr);
	}

//
//	if(err ==0)
//	{
//		RDB_SET_P1_STR(LMP_Status, "Success: collect Y1731 stats");
//	}


	return err;
}
#endif

#endif // NCI_Y1731

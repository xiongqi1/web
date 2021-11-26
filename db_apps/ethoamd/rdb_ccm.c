
#include "rdb_comms.h"
#include "ethlctrl.h"
#include "ethlpoll.h"
#include "tms.h"
#include "utils.h"
#include <string.h>

static const char * cannot_set_ccm_parameter ="Cannot set CCM parameter";

//enable CCM
//$  0 -- success
//$ <0 --- error code
int CCM_enable(Session *pSession)
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
		sprintf(ls->lasterr, "1AG is not configured");
		goto lab_err;
	}

//	TRY_RDB_GET_P1_BOOLEAN(LMP_CCMactive, &l->CCMactive);

	if( ls->CCM_enable_ready==0)
	{

		NTCSLOG_DEBUG("CCM_enable>>>");

		err = MEP_enable(pSession);
		if(err)
		{
			sprintf(ls->lasterr, "cannot setup LMP end point %d", l->mpid);
			goto lab_err;
		}

		TRY_RDB_GET_P1_UINT(MDA_CCMInterval, &m->CCMInterval);
		ETH_SET_PARAM(pSession, 0, MDA_CCMINTERVAL, m->CCMInterval, cannot_set_ccm_parameter, ls->lasterr);


		TRY_RDB_GET_P1_UINT(LMP_CoS, &l->CoS);
		// LMP_CCMPRIORITYDROP is vlan priority bit,COs need left shift: 1->2, 2->4,7 ->0xE

		ETH_SET_PARAM(pSession, l->mpid, LMP_CCMPRIORITYDROP, (l->CoS&0x7)<<1, cannot_set_ccm_parameter, ls->lasterr);


		ETH_SET_PARAM(pSession, l->mpid, LMP_CCMENABLED, 1, cannot_set_ccm_parameter,ls->lasterr);

		ls->CCM_enable_ready =1;

		NTCSLOG_DEBUG("CCM_enable<<<");
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
		RDB_SET_P1_STR(LMP_Status, "Success: CCM is enabled");
	}

	return err;
}

// disable CCM
int CCM_disable(Session *pSession)
{
	int err=0;
	int bok =0;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;
	//mda_data		*m= &pSession->m_mda_data;
	mda_state		*ms= &pSession->m_mda_state;

	if(ls->CCM_enable_ready)
	{
		NTCSLOG_DEBUG("CCM_disable >>>");

		ls->CCM_enable_ready =0;

		ETH_SET_PARAM(pSession, l->mpid, LMP_CCMENABLED, 0, cannot_set_ccm_parameter,ms->lasterr);


		NTCSLOG_DEBUG("CCM_disable <<<");

		goto lab_ok;
	}

lab_err:
	if(err)
	{
		RDB_SET_P1_2STR(LMP_Status, "Error:  ", ms->lasterr);
		NTCSLOG_INFO("failed to disable CCM");
	}
lab_ok:



	if(err ==0)
	{
		RDB_SET_P1_STR(LMP_Status, "Success: CCM is disabled");
	}

	return err;
}




// auto adjust CCM state
//$  1 -- no change
//$  0 -- success, changed
//$ <0 --- error code
int CCM_auto_start(Session *pSession)
{
	if(pSession->m_lmp_state.CCMenabled != pSession->m_lmp_state.CCM_enable_ready)
	{

		if(pSession->m_lmp_state.CCMenabled)
		{
			return CCM_enable(pSession);
		}
		else
		{
			return CCM_disable(pSession);
		}
	}
	return 1;
}

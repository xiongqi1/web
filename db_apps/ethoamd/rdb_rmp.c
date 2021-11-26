
#include <string.h>
#include <ctype.h>
#include "rdb_comms.h"
#include "ethlctrl.h"
#include "ethlpoll.h"
#include "tms.h"
#include "utils.h"
#include "rdbrpc.h"


static const char * cannot_get_rmp_parameter="Cannot get RMP parameter";


static inline rmp_data *RMP_obj(Session *pSession,  int objectid)
{
	if( objectid > 0 && objectid < MAX_RMP_OBJ_NUM)
	{
		return  &pSession->m_rmp_data[objectid];
	}
	return 0;
}

// update rmp._index object
void RMP_update_index(Session *pSession)
{
	int i;
	int idlist[MAX_RMP_OBJ_NUM];
	int n=0;

	for(i =1; i< MAX_RMP_OBJ_NUM; i++)
	{
		rmp_data	*r= &pSession->m_rmp_data[i];
		if(r->rdb_created)
		{
			idlist[n++] = i;
		}
	}
	rdb_set_index_list(RMP_index, pSession->m_node, idlist, n);
}

// allocate RMP slot if it does not exit,
// 1) alloc memory block
// 2) create rdb
//$  0 -- success
//$ <0 --- error code
int RMP_add(Session *pSession,  int objectid)
{
	NTCSLOG_DEBUG("RMP_add(%d) >>>",objectid);
	if( objectid > 0 && objectid < MAX_RMP_OBJ_NUM)
	{
		rmp_data		*r= &pSession->m_rmp_data[objectid];
		if(r)
		{
			memset(r, 0, sizeof(rmp_data));
			// create rdb
			create_rmp_rdb_variables(pSession->m_node, objectid);
			r->rdb_created = 1;
			RMP_update_index(pSession);
			return 0;
			
		}
	}
	NTCSLOG_DEBUG("RMP_add(%d) <<<",objectid);

	return -1;
}

// delete one slot of rmpid
// 1) disabled it
// 2) delete all rdb
// 3) release memory block
// update_index -- weather update index immedietly
//$  0 -- success
//$ <0 --- error code
int RMP_del(Session *pSession, int objectid, int update_index)
{
	
	rmp_data		*r= RMP_obj(pSession, objectid);

	if(r == 0) return -1;

	NTCSLOG_DEBUG("RMP_del(%d), rmpid=%d, tms_enabled=%d >>>", objectid, r->rmpid, r->tms_enabled);

	// disabled it
	if( r->tms_enabled) RMP_disable(pSession, objectid);
	
	// delete all rdbs	
	if(r->rdb_created)delete_rmp_rdb_variables(pSession->m_node, objectid);
	r->rdb_created =0;
	r->tms_enabled =0;
	r->rmpid =0;
	if(update_index)RMP_update_index(pSession);
	return 0;

}

// search rmp list for object id
int RMP_find(Session *pSession,  int rmpid)
{
	int i;
	for(i =1; i< MAX_RMP_OBJ_NUM; i++)
	{
		rmp_data *r= &pSession->m_rmp_data[i];
		if (r->rmpid == rmpid) return i;
	}
	return -1;
}

// assign a object with new RMPID
// 1) make sure it do exist
// 2) set rmpid into rdb
// 3) enable rdb or not
//$  0 -- success
//$ <0 --- error code
int RMP_set(Session *pSession, int objectid, int rmpid, int enable)
{
	int err;
	// make sure it do exist
	NTCSLOG_DEBUG("RMP_set(%d, %d) >>>", objectid, rmpid);
	if(rmpid >=0 && rmpid < MAX_RMP_ID && objectid >0 && objectid < MAX_RMP_OBJ_NUM)
	{
	
		err = RMP_add(pSession, objectid);
		if(err) return err;
		// set rmpid into rdb
		TRY_RDB_SET_P1_2_UINT(RMP_mpid, objectid, rmpid);
		if(enable)
		{
			// enable rdb now
			err = RMP_enable(pSession, objectid);
		}
	}	
lab_err:
	
	NTCSLOG_DEBUG("RMP_set <<<");
	return err;
}

// enable one RMP object
// read rmpid from rdb
// RMPID changed!. if not same and tms_enabled, disabeld it
// if not enable, enable it
//$  0 -- success
//$ <0 --- error code
int RMP_enable(Session *pSession, int objectid)
{
	int err = -1;
	rmp_data		*r= RMP_obj(pSession, objectid);
	if(r == 0) return -1;
	NTCSLOG_DEBUG("RMP_enable(%d) rmpid = %d >>>", objectid, r->rmpid);

	// if not enable, enable it
	if(!r->tms_enabled  && r->rmpid)
	{
		// setup RMP
		err = setupRMP(pSession->m_node, r->rmpid);
		if(err)
		{
			NTCSLOG_WARNING("cannot setup RMP end point %d", r->rmpid);
			goto lab_err;

		}
		r->tms_enabled =1;
	}
	NTCSLOG_DEBUG("RMP_enable(%d) <<<", objectid);

	return 0;

lab_err:

	return err;
}



//////////////////////////////////////////////
int rdb_set_p1_2_LOSSVAL(const char* name, Session *pSession, int j, LOSSVALUE value)
{
    char buf[64];
    int retValue;
    sprintf(buf, "%u:%u", value.nearEnd, value.farEnd);
    retValue = rdb_set_p1_2_str(name, pSession->m_node, j, buf);
    if(retValue)
    {
        return retValue;
    }
    return 0;
}

// collection RMP data into RDB
int RMP_collect(Session *pSession)
{
	int err=0;
	int bok =0;
	int i=0;
	PDURESULT result;
	memset(&result, 0, sizeof(result));
	mda_state		*ms= &pSession->m_mda_state;

	NTCSLOG_DEBUG("rmp_collection >>>");
	for(i =1 ; i< MAX_RMP_OBJ_NUM; i++)
	{
		rmp_data		*r= &pSession->m_rmp_data[i];
		if(r->rdb_created )
		{
			if(r->tms_enabled )
			{
				GET_ETH_PARAM(pSession, r->rmpid, RMP_CCMDEFECT , &r->ccmDefect, cannot_get_rmp_parameter, ms->lasterr);

				GET_ETH_PARAM(pSession, r->rmpid, RMP_LASTRDI, &r->lastccmRDI, cannot_get_rmp_parameter,ms->lasterr);

				GET_ETH_PARAM(pSession, r->rmpid, RMP_LASTPORTSTATE, &r->lastccmPortState, cannot_get_rmp_parameter,ms->lasterr);

				GET_ETH_PARAM(pSession, r->rmpid, RMP_LASTIFSTATUS, &r->lastccmIFStatus, cannot_get_rmp_parameter,ms->lasterr);

				GET_ETH_PARAM(pSession, r->rmpid, RMP_LASTSENDERID, &r->lastccmSenderID, cannot_get_rmp_parameter,ms->lasterr);

				GET_ETH_PARAM(pSession, r->rmpid, RMP_MACADDRESS, &r->lastccmmacAddr, cannot_get_rmp_parameter,ms->lasterr);

				GET_ETH_PARAM(pSession, r->rmpid, RMP_MACADDRESS, &r->lastccmmacAddr, cannot_get_rmp_parameter,ms->lasterr);
		
				GET_ETH_PARAM(pSession, r->rmpid, RMP_TXCOUNTER, &r->TxCounter, cannot_get_rmp_parameter,ms->lasterr);
				
				GET_ETH_PARAM(pSession, r->rmpid, RMP_RXCOUNTER, &r->RxCounter, cannot_get_rmp_parameter,ms->lasterr);
				

	#ifdef NCI_Y1731
				if( ms->Y1731_enable_ready)
				{
					GET_ETH_PARAM(pSession,  r->rmpid, RMP_LM2RESULT, &result, cannot_get_rmp_parameter, ms->lasterr);
				}
			
	#endif
			
			}//if( r->tms_enabled)
			
			TRY_RDB_SET_P1_2_BOOLEAN(RMP_ccmDefect, i, r->ccmDefect);

			TRY_RDB_SET_P1_2_BOOLEAN(RMP_lastccmRDI , i, r->lastccmRDI);

			TRY_RDB_SET_P1_2_UINT(RMP_lastccmPortState , i,r->lastccmPortState);

			TRY_RDB_SET_P1_2_UINT(RMP_lastccmIFStatus ,i, r->lastccmIFStatus);

			TRY_RDB_SET_P1_2_UINT(RMP_lastccmSenderID , i,r->lastccmSenderID);

			TRY_RDB_SET_P1_2_MAC(RMP_lastccmmacAddr ,i, r->lastccmmacAddr);

			TRY_RDB_SET_P1_2_MAC(RMP_lastccmmacAddr ,i, r->lastccmmacAddr);
	
			TRY_RDB_SET_P1_2_UINT(RMP_TxCounter,i, r->TxCounter);
			
			TRY_RDB_SET_P1_2_UINT(RMP_RxCounter,i, r->RxCounter);
			

#ifdef NCI_Y1731
		
			rdb_set_p1_2_LOSSVAL(RMP_Curr, pSession,   i, result.lossResult.curr);
			rdb_set_p1_2_LOSSVAL(RMP_Accum,pSession,   i, result.lossResult.accum);
			rdb_set_p1_2_LOSSVAL(RMP_Ratio,pSession,   i, result.lossResult.ratio);

#endif
		}//if(r->rdb_created )

	}//for(i =1; i< MAX_RMP_OBJ_NUM; i++)

	NTCSLOG_DEBUG("rmp_collection <<<");
	goto lab_ok;

lab_err:
	NTCSLOG_DEBUG("Error: %s", ms->lasterr);

lab_ok:

	return err;
}

// disable one RMP, delete rdb variable
//$  0 -- success
//$ <0 --- error code
int  RMP_disable(Session *pSession, int objectid)
{
	rmp_data		*r= RMP_obj(pSession, objectid);
	if(r == 0) return -1;
	NTCSLOG_DEBUG("RMP_disable(%d) rmpid=%d>>>", objectid, r->rmpid);

	// if neabled, disable it
	if( r->tms_enabled)
	{
		ethlctrl_1agdelrmp(pSession, r->rmpid);
		// reset all field except: rdb_created, rmpid
		r->tms_enabled =0;		//interal 1ag state
		//r->rdb_created;		//external rdb state
		//r->rmpid;				//writeonly
		r->ccmDefect = 0; 			//readonly
		r->lastccmRDI = 0;			//readonly
		r->lastccmPortState = 0;	//readonly
		r->lastccmIFStatus = 0;	//readonly
		r->lastccmSenderID = 0;	//readonly
		r->TxCounter = 0;		//readonly
		r->RxCounter = 0;		//readonly
		memset(r->lastccmmacAddr, 0, sizeof(r->lastccmmacAddr));		//readonly
		r->status = 0;				//readonly
	}
	return 0;

}

// enable all RMP, delete rdb variable
void RMP_enable_all(Session *pSession)
{
	int i;
	NTCSLOG_DEBUG("RMP_enable_all >>>");

	for(i =1; i< MAX_RMP_OBJ_NUM; i++)
	{
		rmp_data		*r= RMP_obj(pSession, i);
		if(!r->tms_enabled  && r->rmpid)
		{
			RMP_enable(pSession, i);
		}
	}
}

// disable one RMP, delete rdb variable
void RMP_disable_all(Session *pSession)
{
	int i;
	NTCSLOG_DEBUG("RMP_disable_all >>>");
	for(i =1; i< MAX_RMP_OBJ_NUM; i++)
	{
		rmp_data		*r= RMP_obj(pSession, i);
		if( r->tms_enabled)
		{
			RMP_disable(pSession, i);
		}
	}
}



// delete all rmp slot and its rdb
void RMP_del_all(Session *pSession)
{
	int i;
	NTCSLOG_DEBUG("RMP_del_all >>>");

	for(i =1; i< MAX_RMP_OBJ_NUM; i++)
	{
		rmp_data		*r=  &pSession->m_rmp_data[i];
		if(r->rdb_created)
		{
			RMP_del(pSession, i, 0);
		}
	}
	RDB_SET_P1_STR(RMP_index, "");
}


#ifdef _RDB_RPC
// send rmp list by RPC
void RMP_rpc_list(Session *pSession)
{
	int i;
	char buf[MAX_RMP_OBJ_NUM*5];
	int len =0;
	buf[0] =0;

	for(i =1; i< MAX_RMP_OBJ_NUM; i++)
	{
		rmp_data	*r= &pSession->m_rmp_data[i];
		if(r->rmpid >0)
		{
			sprintf(&buf[len], "(%d,%d),", i, r->rmpid);
			len = strlen(buf);
		}
	}
	rpc_set_response_msg(buf);
}

#endif
// retrieve RMP rdb data into memory
void RMP_retrieve(Session *pSession)
{
	int i;
	int index[MAX_RMP_OBJ_NUM];
	int n;
	int err;

	// reset all rmp
	memset(pSession->m_rmp_data, 0 , sizeof(pSession->m_rmp_data));

	n = rdb_get_index_list(RMP_index, pSession->m_node , index,MAX_RMP_OBJ_NUM);
	
	NTCSLOG_DEBUG("RMP_retrieve m_node=%d, n=%d>>>",pSession->m_node, n );
	if(n > 0)
	{
		for(i = 0; i< n; i++)
		{
			int rmpid;
			int id  = index[i];
			
			err = RDB_GET_P1_2_INT(RMP_mpid, id, &rmpid);

			if(err ==0 && id < MAX_RMP_OBJ_NUM&& rmpid > 0 )
			{
				rmp_data	*r= &pSession->m_rmp_data[id];
				r->rmpid = rmpid;
				r->rdb_created = 1;
			}
		
		}
	}
}

// sync from RDB object, build RMP object
// 0) rdb doec exist at all, del all RMP
// 1)check rdb exist, objectid exist and rmpid exist
// (1) rdb exist, RMP not exist --  create RMP for it
// (2) rdb exist,  RMP rmpid not same -- disable old rmpid, sync	char  *p = buf;
// 2) rdb  not exit, RMP exist -- delete RMP
void RMP_update(Session *pSession)
{
	int i;
	int index[MAX_RMP_OBJ_NUM];
	int err;
	int n;
	// 0) rdb doec exist at all, del all RMP
	n = err= rdb_get_index_list(RMP_index, pSession->m_node , index,MAX_RMP_OBJ_NUM);
	NTCSLOG_DEBUG("RMP_retrieve m_node=%d, n=%d>>>",pSession->m_node, n );
	if (err<0)
	{
		RMP_del_all(pSession);
		return;
	}
	// 1)check rdb exist, objectid exist and rmpid exist
	// (1) rdb exist, RMP not exist --  create RMP for it
	// (2) rdb exist,  RMP rmpid not same -- disable old rmpid, sync	char  *p = buf;
	for(i = 0; i< n; i++)
	{
		int rmpid;
		int id  = index[i];
		
		err = RDB_GET_P1_2_INT(RMP_mpid, id, &rmpid);
		if(err ==0)
		{
			rmp_data	*r= &pSession->m_rmp_data[id];
			if( r->rmpid <= 0)
			{
				memset(r, 0, sizeof(rmp_data));
			}
			else if(r->rmpid != rmpid)
			{
				RMP_disable(pSession, id);
			}
			r->rmpid = rmpid;
			r->rdb_created = 1;
			RMP_enable(pSession, id);
		}
	
	}
	// 2) rdb  not exit, RMP exist -- delete RMP
	for(i =1; i< MAX_RMP_OBJ_NUM; i++)
	{
		rmp_data	*r= &pSession->m_rmp_data[i];
		if(r->rdb_created)
		{
			int j;
			int rdb_created =0;
			for(j = 0; j<n; j++)
			{
				if (i == index[j])
				{
					// OK
					rdb_created =1;	
					break;
				}
			}
			//
			if ( ! rdb_created) {
				// disabled it
				if( r->tms_enabled) RMP_disable(pSession, i);
				
				r->rdb_created =0;
				r->tms_enabled =0;
				r->rmpid =0;
			}
		
		
		}

	}
	
}



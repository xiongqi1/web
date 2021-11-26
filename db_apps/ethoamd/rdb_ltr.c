

#include "rdb_comms.h"
#include "ethlctrl.h"
#include "ethlpoll.h"
#include "tms.h"
#include "utils.h"
#include <string.h>

// collect ltr parameters
int collect_ltr(Session *pSession)
{
	int err=0;
	int bok =0;

	ltr_data		*b= &pSession->m_ltr_data;
	ltm_data		*t= &pSession->m_ltm_data;
	action_state	*ts= &pSession->m_ltm_state;
	lmp_data		*l= &pSession->m_lmp_data;
	lmp_state		*ls= &pSession->m_lmp_state;

	NTCSLOG_DEBUG("collect_ltr>>>");

		//ETHLPOLL_1AGLTM,	/* Poll a Link Trace Reply Record */
				/* param1 = (int) local MP identifier
				 * param2 = (ulong) transaction ID
				 * param3 = (int) one of ETH1AG_LTMREPLYID
				 * param4 = (ulong) ETH1AG_LTMREPLYID parameter
				 * param5 = (void *) ptr for ETH1AG_LTMREPLY
				 */

	// get param for priority
	//TRY_RDB_GET_P1_INT(LTR_ltmTransId, &b->ltmTransId);

	if(l->mpid ==0)
	{
		err = -1;
		sprintf(ls->lasterr, "LTM is not started");
		goto lab_err;
	}
	while(1)
	{
	//NTCSLOG_DEBUG("ethlpoll_1agltm( mpid=%d, LTMtransID=%d, ltr_num=%d",l->mpid, t->LTMtransID, b->ltr_num );
		bok = ethlpoll_1agltm(pSession, l->mpid, t->LTMtransID, LTM_REPLYINDEX, b->ltr_num, &b->ltmreply[b->ltr_num]);
		if(!bok) {
			break;
		}
		b->ltr_num++;
	}

	if( b->ltr_num ==0)
	{
		err = -1;
		sprintf(ls->lasterr, "no LTR packet");
		goto lab_err;
	}

	goto lab_ok;

lab_err:
	if(err)
	{
		ts->status = err;
	}

lab_ok:
	NTCSLOG_DEBUG("collect_ltr <<<");
	return err;
}

#define LTR_RDB_SET_INT(r, v) buffer[0]=0;\
	for(i =0; i< b->ltr_num; i++)	{\
		sprintf(temp, "%d", b->ltmreply[i].v);		strcat(buffer, temp); strcat(buffer, ",");\
	}	RDB_SET_P1_STR(r, buffer); /*printf(r": %s\n", buffer);*/

#define LTR_RDB_SET_MAC(r, v) buffer[0]=0;\
	for(i =0; i< b->ltr_num; i++)	{\
		MAC2str(temp, b->ltmreply[i].v);		strcat(buffer, temp); strcat(buffer, ",");\
	}	RDB_SET_P1_STR(r, buffer); /*printf(r": %s\n", buffer);*/

#define LTR_RDB_SET_STR(r, v) buffer[0]=0;\
	for(i =0; i< b->ltr_num; i++)	{\
		strcat(buffer, (const char*)b->ltmreply[i].v);strcat(buffer, ",");\
	}	printf(r": %s\n", buffer);

#define LTR_RDB_SET_SHORT_MAC(r, v) buffer[0]=0;\
	for(i =0; i< b->ltr_num; i++)	{\
		char temp1[32];		MAC2str(temp1, &(b->ltmreply[i].v)[2]);\
		sprintf(temp, "{%d,%s}", *((short*)b->ltmreply[i].v), temp1);\
		strcat(buffer, temp);strcat(buffer, ",");\
	}	RDB_SET_P1_STR(r, buffer); /*printf(r": %s\n", buffer);*/

#define LTR_RDB_SET_BYTE(r, v, l) buffer[0]=0;\
	for(i =0; i< b->ltr_num; i++)	{\
		unsigned char j;\
		for(j=0;j< b->ltmreply[i].l; j++) 	{\
			sprintf(temp, "%02x", (unsigned int)b->ltmreply[i].v[j]);\
			strcat(buffer, temp);	 }\
		strcat(buffer, ",");\
	}	printf(r": %s\n", buffer);


#define LTR_RDB_SET_INT_EX(r, v, l, s) buffer[0]=0;\
	for(i =0; i< b->ltr_num; i++)	{\
		if( b->ltmreply[i].l > s)	{\
			sprintf(temp, "%d", b->ltmreply[i].v);\
			strcat(buffer, temp);		}\
				 strcat(buffer, ",");\
	}	printf(r": %s\n", buffer);

#define LTR_RDB_SET_BYTE_EX(r, v, l, s, m) buffer[0]=0;\
	for(i =0; i< b->ltr_num; i++)	{\
		unsigned char j; 	int num = 0;\
		if( b->ltmreply[i].l > s)	{\
			num = b->ltmreply[i].l-s;\
			num  = num > m? m:num;		}\
		for(j=0;j< num  ; j++) 	{\
			sprintf(temp, "%02x", (unsigned int)b->ltmreply[i].v[j]);\
			strcat(buffer, temp);	 }\
		strcat(buffer, ",");\
	}	printf(r": %s\n", buffer);

#define LTR_APPEND_BYTE(v, n)\
	for(j=0;j< (n) ; j++) 	{\
		sprintf(temp, "%02x", *((&b->ltmreply[i].v)+j) );\
		strcat(buffer, temp);	}

// update ltr rdb
int update_rdb_ltr(Session *pSession)
{
	int err=0;
	int i=0;

	char buffer[1024];
	char temp[32];
	ltr_data		*b= &pSession->m_ltr_data;

	ltm_data		*t= &pSession->m_ltm_data;

	NTCSLOG_DEBUG("update_rdb_ltr>>> b->ltr_num=%d", b->ltr_num);

	RDB_SET_P1_INT(LTR_ltmTransId, t->LTMtransID);

	RDB_SET_P1_INT(LTR_rmpid, t->rmpid);

	LTR_RDB_SET_INT(LTR_relayaction, ltrRelayAction);
	LTR_RDB_SET_MAC(LTR_srcmac, ltrReplyIngress.ingressMAC);

	LTR_RDB_SET_INT(LTR_flag, ltrFlags);

	LTR_RDB_SET_INT(LTR_ttl, ltrReplyTTL);



#if 0 // solution 2
	LTR_RDB_SET_MAC(LTR_destmac, macAdr);
	LTR_RDB_SET_INT(LTR_ltrFlags, ltrFlags);
	LTR_RDB_SET_INT(LTR_ltrRelayAction, ltrRelayAction);
	LTR_RDB_SET_INT(LTR_LTRttl, ltrReplyTTL);
	LTR_RDB_SET_SHORT_MAC(LTR_ltrEgressLast, ltrEgress.lastEgressID);
	LTR_RDB_SET_SHORT_MAC(LTR_ltrEgressNext, ltrEgress.nextEgressID);
#endif

#if 0 // solution 1
	LTR_RDB_SET_INT(LTR_Ingress_action, ltrReplyIngress.action);
	LTR_RDB_SET_MAC(LTR_Ingress_macAddr, ltrReplyIngress.ingressMAC);
	LTR_RDB_SET_INT(LTR_Ingress_portIdSubType, ltrReplyIngress.portIDsubtype);
	ltr_rdb_set_byte(LTR_Ingress_portId, ltrReplyIngress.portID, ltrReplyIngress.portIDlen);


	LTR_RDB_SET_INT(LTR_Egress_action, ltrReplyEgress.action);
	LTR_RDB_SET_MAC(LTR_Egress_macAddr, ltrReplyEgress.egressMAC);
	LTR_RDB_SET_INT(LTR_Egress_portIdSubType, ltrReplyEgress.portIDsubtype);
	ltr_rdb_set_byte(LTR_Egress_portId, ltrReplyEgress.portID,ltrReplyEgress.portIDlen);

	LTR_RDB_SET_INT(LTR_chassIdSubtype, ltrSenderID.chassIDsubtype);
	ltr_rdb_set_byte(LTR_chassId, ltrSenderID.chassID,ltrSenderID.chassIDlength);
	ltr_rdb_set_byte(LTR_mdId, ltrSenderID.mgmtDom, ltrSenderID.mgmtDomLength);
	ltr_rdb_set_byte(LTR_maId, ltrSenderID.mgmtAdr,ltrSenderID.mgmtAdrLength);


	ltr_rdb_set_byte_ex(LTR_oui, ltrOrgSpec.oui, ltrOrgSpec.totalLen, 0, 3);
	ltr_rdb_set_byte_ex(LTR_subtype, ltrOrgSpec.oui, ltrOrgSpec.totalLen, 3, 1);
	ltr_rdb_set_byte_ex(LTR_msg, ltrOrgSpec.oui, ltrOrgSpec.totalLen,4,(1480-4));
#endif
#if 0 // solution 2
	//ingress
	buffer[0]=0;
	for(i =0; i< b->ltr_num; i++)
	{
		unsigned char j;
		strcat(buffer, "{");
		if(b->ltmreply[i].ltrReplyIngress.action != 0)
		{
			ltr_append_byte(ltrReplyIngress.action, 1)
			strcat(buffer, ",");
			MAC2str(temp,b->ltmreply[i].ltrReplyIngress.ingressMAC);
			strcat(buffer, temp);
			strcat(buffer, ",");
			if( b->ltmreply[i].ltrReplyIngress.portIDlen >0)
			{
				ltr_append_byte(ltrReplyIngress.portIDsubtype, 1);
				strcat(buffer, ",");
				ltr_append_byte(ltrReplyIngress.portID[0], b->ltmreply[i].ltrReplyIngress.portIDlen);
			}
			else
			{
				strcat(buffer, ",");
			}
		}
		else
		{
			strcat(buffer, ",,,");
		}
		strcat(buffer, "}");
		if(i !=  (b->ltr_num-1))strcat(buffer, ",");
	}
	//printf(LTR_ingress": %s\n", buffer);
	RDB_SET_P1_STR(LTR_ingress, buffer);
	//egress
	buffer[0]=0;
	for(i =0; i< b->ltr_num; i++)
	{
		unsigned char j;
		strcat(buffer, "{");
		if(b->ltmreply[i].ltrReplyEgress.action != 0)
		{
			ltr_append_byte(ltrReplyEgress.action, 1)
			strcat(buffer, ",");
			MAC2str(temp,b->ltmreply[i].ltrReplyEgress.egressMAC);
			strcat(buffer, temp);
			strcat(buffer, ",");
			if( b->ltmreply[i].ltrReplyEgress.portIDlen >0)
			{
				ltr_append_byte(ltrReplyEgress.portIDsubtype, 1);
				strcat(buffer, ",");
				ltr_append_byte(ltrReplyEgress.portID[0], b->ltmreply[i].ltrReplyEgress.portIDlen);
			}
			else
			{
				strcat(buffer, ",");
			}
		}
		else
		{
			strcat(buffer, ",,,");
		}
		strcat(buffer, "}");
		if(i !=  (b->ltr_num-1))strcat(buffer, ",");
	}
	//printf(LTR_egress": %s\n", buffer);
	RDB_SET_P1_STR(LTR_egress, buffer);
	// senderid
	buffer[0]=0;
	for(i =0; i< b->ltr_num; i++)
	{
		unsigned char j;
		strcat(buffer, "{");
		if(b->ltmreply[i].ltrSenderID.chassIDlength>0)
		{
			ltr_append_byte(ltrSenderID.chassIDsubtype, 1)
			strcat(buffer, ",");
			ltr_append_byte(ltrSenderID.chassID[0], b->ltmreply[i].ltrSenderID.chassIDlength)
			strcat(buffer, ",");
		}
		else
		{
			strcat(buffer, ",,");
		}
		ltr_append_byte(ltrSenderID.mgmtDom[0], b->ltmreply[i].ltrSenderID.mgmtDomLength);
		strcat(buffer, ",");
		ltr_append_byte(ltrSenderID.mgmtAdr[0],b->ltmreply[i].ltrSenderID.mgmtAdrLength);
		strcat(buffer, "}");
		if(i !=  (b->ltr_num-1))strcat(buffer, ",");
	}
	///printf(LTR_senderid": %s\n", buffer);
	RDB_SET_P1_STR(LTR_senderid, buffer);

	buffer[0]=0;
	for(i =0; i< b->ltr_num; i++)
	{
		unsigned char j;
		strcat(buffer, "{");
		if( b->ltmreply[i].ltrOrgSpec.totalLen >= 3)
		{
			ltr_append_byte(ltrOrgSpec.oui[0], 3);
		}
		strcat(buffer, ",");
		if( b->ltmreply[i].ltrOrgSpec.totalLen >= 4)
		{
			ltr_append_byte(ltrOrgSpec.oui[3], 1);
		}
		strcat(buffer, ",");

		if( b->ltmreply[i].ltrOrgSpec.totalLen >= 5)
		{
			ltr_append_byte(ltrOrgSpec.oui[4], b->ltmreply[i].ltrOrgSpec.totalLen-4);
		}
		strcat(buffer, "}");
		if(i !=  (b->ltr_num-1))strcat(buffer, ",");
	}
	//printf(LTR_orgspec": %s\n", buffer);
	RDB_SET_P1_STR(LTR_orgspec, buffer);
#endif

	// solution 3


	NTCSLOG_DEBUG("update_rdb_ltr<<<");
	return err;

}

#include "sierracns.h"

#include <time.h>
#include <arpa/inet.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierracns_emptyList(sierracns* pCns)
{
	// free perm list
	while (!list_empty(&pCns->permList))
	{
		permcnsentry* pPermEntry = list_entry(pCns->permList.next, permcnsentry, list);

		list_del(&pPermEntry->list);

		__free(pPermEntry->pCnsPck);
		__free(pPermEntry);
	}

	// free queue list
	while (!list_empty(&pCns->queueHdr))
	{
		cnsentry* pCnsEntry = list_entry(pCns->queueHdr.next, cnsentry, list);

		list_del(&pCnsEntry->list);

		__free(pCnsEntry->pCnsPck);
		__free(pCnsEntry);
	}

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierracns_destroy(sierracns* pCns)
{
	__bypassIfNull(pCns);

	sierracns_emptyList(pCns);

	// destroy sierra hip
	sierrahip_destroy(pCns->pHip);

	// destroy object
	__free(pCns);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierracns_writeNotifyEnable(sierracns* pCns, unsigned short wObjId)
{
	return sierracns_write(pCns, wObjId, SIERRACNS_PACKET_OP_NOTIFY_ENABLE, NULL, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierracns_writeGet(sierracns* pCns, unsigned short wObjId)
{
	return sierracns_write(pCns, wObjId, SIERRACNS_PACKET_OP_GET, NULL, 0);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierracns_writeSet(sierracns* pCns, unsigned short wObjId)
{
	return sierracns_write(pCns, wObjId, SIERRACNS_PACKET_OP_SET, NULL, 0);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
sierracns_hdr* sierracns_allocCns(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam, int* pCnsLen)
{
	sierracns_hdr* pCnsHdr = NULL;
	int cbCnsLen = sizeof(sierracns_hdr) + cbParam;
	void* pCnsParam;

	// returns error if excessed
	__goToErrorIfFalse(cbParam <= SIERRACNS_PACKET_PARAMMAXLEN);

	// allocate cns
	__goToErrorIfFalse(pCnsHdr = __alloc(cbCnsLen));

	// build header
	pCnsHdr->wObjectId = htons(wObjId);
	pCnsHdr->bOpType = bOpType;
	pCnsHdr->dwAppId = htonl(1);
	pCnsHdr->wParamLen = htons(cbParam);

	// copy param
	pCnsParam = __getNextPtr(pCnsHdr);
	memcpy(pCnsParam, pParam, cbParam);

	// set length
	if (pCnsLen)
		*pCnsLen = cbCnsLen;

	return pCnsHdr;

error:
	__free(pCnsHdr);
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierracns_addPerm(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam, int repeatSec, BOOL fStopIfError)
{
	permcnsentry* pPermEntry;

	// allocate perm entry
	__goToErrorIfFalse(pPermEntry = (permcnsentry*)__alloc(sizeof(permcnsentry)));

	pPermEntry->nRepeatSec = repeatSec;
	pPermEntry->fStopIfError=fStopIfError;

	// get get cns packet
	__goToErrorIfFalse(pPermEntry->pCnsPck = sierracns_allocCns(pCns, wObjId, bOpType, pParam, cbParam, &pPermEntry->cbCnsPck));

	// add to tail
	list_add_tail(&pPermEntry->list, &pCns->permList);

	return TRUE;

error:
	// free cns packet
	if (pPermEntry)
		__free(pPermEntry->pCnsPck);

	// free perm entry
	__free(pPermEntry);
	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierracns_addPermGet(sierracns* pCns, unsigned short wObjId, int repeatSec, int fStopIfError)
{
	return sierracns_addPerm(pCns, wObjId, SIERRACNS_PACKET_OP_GET, NULL, 0, repeatSec, fStopIfError);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierracns_writeInt(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam, void* pUsrRef)
{
	cnsentry* pCnsEntry=NULL;
	
	syslog(LOG_DEBUG,"##cns## req wObjId=0x%04x",wObjId);

	__goToErrorIfFalse(pCns);

	// allocate cns entry
	__goToErrorIfFalse(pCnsEntry = (cnsentry*)__alloc(sizeof(cnsentry)));

	// get get cns packet
	__goToErrorIfFalse(pCnsEntry->pCnsPck = sierracns_allocCns(pCns, wObjId, bOpType, pParam, cbParam, &pCnsEntry->cbCnsPck));

	pCnsEntry->pUsrRef=pUsrRef;

	// add to tail
	list_add_tail(&pCnsEntry->list, &pCns->queueHdr);

	return TRUE;

error:
	// free cns packet
	__free(pCnsEntry->pCnsPck);
	// free cns entry
	__free(pCnsEntry);
	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierracns_write(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pParam, int cbParam)
{

#ifdef GPS_PKT_DEBUG
if (wObjId == SIERRACNS_PACKET_OBJ_INITIATE_LOCATION_FIX) {
    syslog(LOG_ERR, "sierracns_write(SIERRACNS_PACKET_OBJ_INITIATE_LOCATION_FIX 0x%04x)",wObjId);
} else if (wObjId == SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION) {
    syslog(LOG_ERR, "sierracns_write(SIERRACNS_PACKET_OBJ_START_TRACKING_SESSION 0x%04x)",wObjId);
} else if (wObjId == SIERRACNS_PACKET_OBJ_END_LOCATION_FIX_SESSION) {
    syslog(LOG_ERR, "sierracns_write(SIERRACNS_PACKET_OBJ_END_LOCATION_FIX_SESSION 0x%04x)",wObjId);
}
#endif

	return sierracns_writeInt(pCns, wObjId, bOpType, pParam, cbParam, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierracns_setCnsHandler(sierracns* pCns, SIERRACNS_CNSHANDLER* lpfnCnsHandler)
{
	pCns->lpfnCnsHandler = lpfnCnsHandler;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cnsentry* sierracns_getFirstCnsEntry(sierracns* pCns)
{
	if (list_empty(&pCns->queueHdr))
		return NULL;

	return list_entry(pCns->queueHdr.next, cnsentry, list);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierracns_callCnsHandler(sierracns* pCns, unsigned short wObjId, unsigned char bOpType, void* pPayLoad, int cbPayLoad, sierracns_stat cnsStat)
{
	SIERRACNS_CNSHANDLER* lpfnCnsHandler;

	if (__isAssigned(lpfnCnsHandler = pCns->lpfnCnsHandler))
		lpfnCnsHandler(pCns, wObjId, bOpType, pPayLoad, cbPayLoad, cnsStat);

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierracns_onTick(sierracns* pCns)
{
	cnsentry* pCnsEntry;
	clock_t tickNow;
	struct list_head* pListEntry;

	int nTicksPerSec = __getTicksPerSecond();

	tickNow = __getTickCount();

	// process if any entry existing
	if (__isAssigned(pCnsEntry = sierracns_getFirstCnsEntry(pCns)))
	{
		BOOL fTimeOut = tickNow - pCnsEntry->tickSent >= SIERRACNS_PACKET_TIMEOUT * nTicksPerSec;

		if (pCnsEntry->nTryCnt >= SIERRACNS_PACKET_RETRY && fTimeOut)
		{
			sierracns_hdr* pCnsHdr = pCnsEntry->pCnsPck;
			void* pCnsParam = ntohs(pCnsHdr->wParamLen) ? __getNextPtr(pCnsHdr) : NULL;

			// call handler for timeout failure
			sierracns_callCnsHandler(pCns, ntohs(pCnsHdr->wObjectId), pCnsHdr->bOpType, pCnsParam, ntohs(pCnsHdr->wParamLen), sierracns_stat_timeout);

			// delete cns entry
			list_del(&pCnsEntry->list);
			__free(pCnsEntry->pCnsPck);
			__free(pCnsEntry);
		}
		else if (!pCnsEntry->nTryCnt || fTimeOut)
		{
			sierrahip_writeHip(pCns->pHip, SIERRACNS_PACKET_MSGID_HOST, 0, pCnsEntry->pCnsPck, pCnsEntry->cbCnsPck);

			// update sent time and increase try count
			pCnsEntry->nTryCnt++;
			pCnsEntry->tickSent = tickNow;
		}
	}

	// add if any perm cns is expired
	list_for_each(pListEntry, &pCns->permList)
	{
		permcnsentry* pPermEntry = container_of(pListEntry, permcnsentry, list);
		sierracns_hdr* pCnsPck = pPermEntry->pCnsPck;
		void* pCnsParam = ntohs(pCnsPck->wParamLen) ? __getNextPtr(pCnsPck) : NULL;

		// stop sending if error occurs
		if(pPermEntry->fStopIfError && pPermEntry->fError)
			continue;

		if (!pPermEntry->fSent || tickNow - pPermEntry->tickInsert >= pPermEntry->nRepeatSec*nTicksPerSec)
		{
			sierracns_writeInt(pCns, ntohs(pCnsPck->wObjectId), pCnsPck->bOpType, pCnsParam, ntohs(pCnsPck->wParamLen), pPermEntry);
			pPermEntry->tickInsert = tickNow;
		}

		pPermEntry->fSent = TRUE;
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierracns_callBackOnHip(sierrahip* pHip, unsigned char bMsgId, unsigned char bParam, void* pPayLoad, unsigned short cbPayLoad, void* pRef)
{
	sierracns* pCns = (sierracns*)pRef;
	sierracns_hdr* pCnsHdr = (sierracns_hdr*)pPayLoad;
	unsigned char bOpType;
	BOOL fErrorRes;

	unsigned short wObjId;

	void* pCnsParam = __getNextPtr(pCnsHdr);

	// do a validate check
	__goToErrorIfFalse((bMsgId == SIERRACNS_PACKET_MSGID_MODEM) && (cbPayLoad >= sizeof(sierracns_hdr)) && (ntohs(pCnsHdr->wParamLen) <= SIERRACNS_PACKET_PARAMMAXLEN));

	wObjId = ntohs(pCnsHdr->wObjectId);

	fErrorRes = __isTrue(pCnsHdr->bOpType & 0x80);
	bOpType = pCnsHdr->bOpType & ~0x80;

	// if notification
	if (bOpType == SIERRACNS_PACKET_OP_NOTIFICATION)
	{
		sierracns_callCnsHandler(pCns, wObjId, pCnsHdr->bOpType, pCnsParam, ntohs(pCnsHdr->wParamLen), sierracns_stat_success);
	}
	// if response
	else if (__isFalse(bOpType & 0x01) && bOpType < SIERRACNS_PACKET_OP_NOTIFICATION)
	{
		cnsentry* pQueuedCnsEntry;
		BOOL fCallHandler = FALSE;

		// remove entry if object is identical
		if (__isAssigned(pQueuedCnsEntry = sierracns_getFirstCnsEntry(pCns)))
		{
			sierracns_hdr* pQueuedCnsHdr = pQueuedCnsEntry->pCnsPck;
			unsigned char bExpOp = pQueuedCnsHdr->bOpType + 1;

			if (bExpOp == bOpType && ntohs(pQueuedCnsHdr->wObjectId) == wObjId)
			{
				permcnsentry* pPermEntry=(permcnsentry*)pQueuedCnsEntry->pUsrRef;

				if(pPermEntry && pPermEntry->fStopIfError)
					pPermEntry->fError=fErrorRes;

				list_del(&pQueuedCnsEntry->list);
				__free(pQueuedCnsEntry->pCnsPck);
				__free(pQueuedCnsEntry);

				fCallHandler = TRUE;
			}
		}

		if (fCallHandler)
		{
			sierracns_stat cnsStat = fErrorRes ? sierracns_stat_failure : sierracns_stat_success;
			sierracns_callCnsHandler(pCns, wObjId, pCnsHdr->bOpType, pCnsParam, ntohs(pCnsHdr->wParamLen), cnsStat);
		}
	}

	return;

error:
	return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierracns_closeDev(sierracns* pCns)
{
	sierrahip_closeHipDev(pCns->pHip);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierracns_openDev(sierracns* pCns, const char* lpszDev)
{
	return sierrahip_openHipDev(pCns->pHip, lpszDev);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
sierracns* sierracns_create(void)
{
	sierracns* pCns;

	// create object
	__goToErrorIfFalse(pCns = __allocObj(sierracns));

	INIT_LIST_HEAD(&pCns->queueHdr);
	INIT_LIST_HEAD(&pCns->permList);

	// create sierra hip
	__goToErrorIfFalse(pCns->pHip = sierrahip_create());

	// initiate hip
	sierrahip_setHipHandler(pCns->pHip, sierracns_callBackOnHip, pCns);

	return pCns;

error:
	sierracns_destroy(pCns);
	return NULL;
}

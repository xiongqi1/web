#include "funccaller.h"

#include <stdlib.h>
#include <string.h>
#include <syslog.h>

////////////////////////////////////////////////////////////////////////////////
int funccaller_callQueryCallback(funccaller* pF, callback_entry* pE, tick tckCur)
{
	pE->tickQuery = tckCur;

	if (pE->pCallbackQuery)
		return pE->pCallbackQuery(pF, pE);

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
int funccaller_callAnsCallback(funccaller* pF, callback_entry* pE)
{
	if (pE->pCallbackAns)
		return pE->pCallbackAns(pF, pE);

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
static void funccaller_deleteEnt(funccaller* pF, callback_entry* pEnt)
{
	list_del(&pEnt->listHdr);
	free(pEnt);
}
////////////////////////////////////////////////////////////////////////////////
void funccaller_destroy(funccaller* pF)
{
	if (!pF)
		return;

	// delete until empty
	while (!list_empty(&pF->listHdr))
	{
		callback_entry* pE = list_entry(pF->listHdr.next, callback_entry, listHdr);
		funccaller_deleteEnt(pF, pE);
	}
}

////////////////////////////////////////////////////////////////////////////////
static callback_entry* funccaller_allocEnt(funccaller* pF, funccaller_callback* pCallbackQuery, funccaller_callback* pCallbackAns, callback_entry_type entType, const void* pRef, struct list_head* pLstHdr)
{
	callback_entry* pE;

	pE = malloc(sizeof(*pE));
	if (!pE)
		goto error;
	memset(pE, 0, sizeof(*pE));

	INIT_LIST_HEAD(&pE->listHdr);

	pE->pCallbackQuery = pCallbackQuery;
	pE->pCallbackAns = pCallbackAns;
	pE->entType = entType;
	pE->pRef = pRef;

	list_add(&pE->listHdr, pLstHdr);

	return pE;

error:
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
callback_entry* funccaller_getEntry(funccaller* pF)
{
	callback_entry* pCurEnt = pF->pCurEnt;

	// skip the header
	int fWrapped = &pCurEnt->listHdr == &pF->listHdr;
	if (fWrapped)
		pCurEnt = list_entry(pCurEnt->listHdr.next, callback_entry, listHdr);

	if (&pCurEnt->listHdr == &pF->listHdr)
		return NULL;

	return pF->pCurEnt = pCurEnt;
}
////////////////////////////////////////////////////////////////////////////////
callback_entry* funccaller_moveOn(funccaller* pF, int fSuccess, int* pWrapped)
{
	callback_entry* pCurEnt = pF->pCurEnt;

	if (&pCurEnt->listHdr == &pF->listHdr)
		return NULL;

	// get the entry to delete
	callback_entry* pEntToDel = NULL;
	if ((fSuccess && pCurEnt->entType == callback_entry_type_deleteonsuccess) || (!fSuccess && pCurEnt->entType == callback_entry_type_deleteonfailure) || pCurEnt->entType == callback_entry_type_deleteonfinish)
		pEntToDel = pCurEnt;

	// get the next entry
	pCurEnt = list_entry(pCurEnt->listHdr.next, callback_entry, listHdr);

	// apply new entry
	pF->pCurEnt = pCurEnt;

	// delete if needed
	if (pEntToDel)
		funccaller_deleteEnt(pF, pEntToDel);

	int fWrapped = &pF->pCurEnt->listHdr == &pF->listHdr;
	if (pWrapped)
		*pWrapped = fWrapped;

	return pF->pCurEnt;
}
////////////////////////////////////////////////////////////////////////////////
callback_entry* funccaller_insert(funccaller* pF, funccaller_callback* pCallbackQuery, funccaller_callback* pCallbackAns, callback_entry_type entType, const void* pRef)
{
	struct list_head* pL = &pF->listHdr;

	return funccaller_allocEnt(pF, pCallbackQuery, pCallbackAns, entType, pRef, pL);
};
////////////////////////////////////////////////////////////////////////////////
callback_entry* funccaller_insert_before(funccaller* pF, funccaller_callback* pCallbackQuery, funccaller_callback* pCallbackAns, callback_entry_type entType, const void* pRef, const callback_entry* pEnt)
{
	struct list_head* pL = pEnt->listHdr.prev;

	return funccaller_allocEnt(pF, pCallbackQuery, pCallbackAns, entType, pRef, pL);
}

////////////////////////////////////////////////////////////////////////////////
callback_entry* funccaller_append(funccaller* pF, funccaller_callback* pCallbackQuery, funccaller_callback* pCallbackAns, callback_entry_type entType, const void* pRef)
{
	struct list_head* pL = pF->listHdr.prev;

	return funccaller_allocEnt(pF, pCallbackQuery, pCallbackAns, entType, pRef, pL);
}
////////////////////////////////////////////////////////////////////////////////
funccaller* funccaller_create(tick tckTimeOutMS)
{
	funccaller* pF;

	// allocate obj
	pF = malloc(sizeof(*pF));
	if (!pF)
	{
		syslog(LOG_ERR, "failed to allocate memory for a function caller object");
		goto error;
	}
	memset(pF, 0, sizeof(*pF));

	// init memeber variables
	INIT_LIST_HEAD(&pF->listHdr);
	pF->pCurEnt = (callback_entry*) & pF->listHdr;
	pF->tckTimeOutMS = tckTimeOutMS;

	return pF;

error:
	funccaller_destroy(pF);
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void funccaller_setTimeOut(funccaller* pF, callback_entry* pE, tick tckCur)
{
	pE->tickQuery = tckCur;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int funccaller_isTimeOut(funccaller* pF, callback_entry* pE, tick tckCur)
{
	return pE->tickQuery + pF->tckTimeOutMS < tckCur;
}

#include "objectlist.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>


///////////////////////////////////////////////////////////////////////////////
void objectlist_destructorNull(void* pMem)
{
}
///////////////////////////////////////////////////////////////////////////////
int objectlist_compareString(const void* pA, const void* pB)
{
	return strcmp((const char*)pA, (const char*)pB);
}
///////////////////////////////////////////////////////////////////////////////
static void objectlist_defaultDestructor(void* pM)
{
	if (pM)
		free(pM);
}

///////////////////////////////////////////////////////////////////////////////
void objectlist_delObj(objectlist* pObjList, objectlistelement* pE)
{
	if (!pE)
		return;

	if (pE->list.next && pE->list.prev)
		list_del(&pE->list);

	objectlist_destructor* pDestructor = objectlist_defaultDestructor;
	if (pE->pDestructor)
		pDestructor = pE->pDestructor;

	pDestructor(pE->pM);

	free(pE);
}
///////////////////////////////////////////////////////////////////////////////
static objectlistelement* objectlist_addObjOrMem(objectlist* pObjList, void* pObj, objectlist_destructor* pDestructor, int cbMem)
{
	objectlistelement* pE;

	pE = malloc(sizeof(*pE));
	if (!pE)
		goto error;

	memset(pE, 0, sizeof(*pE));

	pE->pM = pObj;
	pE->pDestructor = pDestructor;

	if (!pObj && cbMem)
	{
		pE->pM = malloc(cbMem);
		if (!pE->pM)
			goto error;

		memset(pE->pM, 0, cbMem);
	}

	list_add_tail(&pE->list, &pObjList->objHdr);

	return pE;

error:
	objectlist_delObj(pObjList, pE);
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////
objectlistelement* objectlist_addObj(objectlist* pObjList, void* pObj, objectlist_destructor* pDestructor)
{
	return objectlist_addObjOrMem(pObjList, (void*)pObj, pDestructor, 0);
}
///////////////////////////////////////////////////////////////////////////////
objectlistelement* objectlist_addDummyObj(objectlist* pObjList, int cbLen)
{
	return objectlist_addObjOrMem(pObjList, NULL, NULL, cbLen);
}
///////////////////////////////////////////////////////////////////////////////
void* objectlist_lookUp(objectlist* pObjList, const void* pObj, objectlist_compare* lpfnCompare)
{
	if (list_empty(&pObjList->objHdr))
		return NULL;

	struct list_head* pPtr;
	list_for_each(pPtr, &pObjList->objHdr)
	{
		objectlistelement* pE = list_entry(pPtr, objectlistelement, list);

		if (!lpfnCompare(pObj, pE->pM))
			return pE->pM;
	}

	return NULL;
}
///////////////////////////////////////////////////////////////////////////////
void* objectlist_findNext(objectlist* pObjList)
{

	struct list_head* pCur = pObjList->pCurSearch->next;
	if (pCur == &pObjList->objHdr)
		return NULL;

	pObjList->pCurSearch = pCur;

	objectlistelement* pE = list_entry(pCur, objectlistelement, list);
	return pE->pM;
}

///////////////////////////////////////////////////////////////////////////////
void* objectlist_findFirst(objectlist* pObjList)
{
	pObjList->pCurSearch = &pObjList->objHdr;
	return objectlist_findNext(pObjList);
}

///////////////////////////////////////////////////////////////////////////////
void objectlist_clear(objectlist* pObjList)
{
	while (!list_empty(&pObjList->objHdr))
	{
		struct list_head* pN = pObjList->objHdr.next;
		objectlistelement* pE = list_entry(pN, objectlistelement, list);
		objectlist_delObj(pObjList, pE);
	}
}
///////////////////////////////////////////////////////////////////////////////
void objectlist_destroy(objectlist* pObjList)
{
	if (!pObjList)
		return;

	objectlist_clear(pObjList);

	free(pObjList);
}

///////////////////////////////////////////////////////////////////////////////
objectlist* objectlist_create(void)
{
	objectlist* pObjList;

	pObjList = malloc(sizeof(*pObjList));
	if (!pObjList)
	{
		errno = ENOMEM;
		goto error;
	}

	memset(pObjList, 0, sizeof(*pObjList));

	INIT_LIST_HEAD(&pObjList->objHdr);

	return pObjList;

error:
	objectlist_destroy(pObjList);
	return NULL;
}

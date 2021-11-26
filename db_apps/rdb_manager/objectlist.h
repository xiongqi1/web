#ifndef __OBJECTLIST_H__
#define __OBJECTLIST_H__

#include "list.h"

typedef void (objectlist_destructor)(void* pObj);
typedef int (objectlist_compare)(const void* pObjA, const void* pObjB);

///////////////////////////////////////////////////////////////////////////////
typedef struct
{
	struct list_head list;

	void* pM;

	objectlist_destructor* pDestructor;

} objectlistelement;

///////////////////////////////////////////////////////////////////////////////
typedef struct
{
	struct list_head objHdr;
	void* pObj;

	struct list_head* pCurSearch;
} objectlist;

///////////////////////////////////////////////////////////////////////////////
objectlist* objectlist_create(void);
void objectlist_destroy(objectlist* pObjList);

objectlistelement* objectlist_addDummyObj(objectlist* pObjList, int cbLen);
objectlistelement* objectlist_addObj(objectlist* pObjList, void* pObj, objectlist_destructor* pDestructor);
void objectlist_delObj(objectlist* pObjList, objectlistelement* ppM);

void objectlist_clear(objectlist* pObjList);

void* objectlist_findFirst(objectlist* pObjList);
void* objectlist_findNext(objectlist* pObjList);
void* objectlist_lookUp(objectlist* pObjList, const void* pObj, objectlist_compare* lpfnCompare);


void objectlist_destructorNull(void* pMem);
int objectlist_compareString(const void* pA, const void* pB);

#endif

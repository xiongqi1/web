#ifndef __DBENUM_H__
#define __DBENUM_H__

#include "base.h"
#include "growingmem.h"


#define DBENUM_GROWING_NAME_BUFFER	1024
#define DBENUM_GROWING_NAME_COUNT		64

typedef struct
{
	char* szName;
	void* pUserPtr;
} dbenumitem;

typedef struct
{
	int nFlags;

	pagedmem* pDbNameStrMem;

	int cDbNameArray;
	pagedmem* pDbNameArrayMem;

	int iCurSearch;
} dbenum;


void dbenum_destroy(dbenum* pEnum);
dbenum* dbenum_create(int nFlags);

int dbenum_chopToArray(dbenum* pEnum, BOOL fCountOnly);
int dbenum_getVarCount(dbenum* pEnum);
int dbenum_enumDb(dbenum* pEnum);

dbenumitem* dbenum_getItem(dbenum* pEnum, const char* szVarName);

dbenumitem* dbenum_findNext(dbenum* pEnum);
dbenumitem* dbenum_findFirst(dbenum* pEnum);

#endif

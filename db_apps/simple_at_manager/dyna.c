#include <stdlib.h>
#include <string.h>

#include "dyna.h"

#define DYNA_FOURCC(a,b,c,d)				((a)<<(0*8) | (b)<<(1*8) | (c)<<(2*8) | (d)<<(3*8))
#define DYNA_MAGICKEY								DYNA_FOURCC('d','y','n','a')

///////////////////////////////////////////////////////////////////////////////
static void dynaDefFree(struct dyna* pDO)
{
	free(pDO);
}
///////////////////////////////////////////////////////////////////////////////
static void* dynaGetObj(struct dyna* pDO)
{
	if (!pDO)
		return 0;

	return pDO + 1;
}
///////////////////////////////////////////////////////////////////////////////
static int dynaIsDyna(struct dyna* pDO)
{
	return pDO->dwMagicKey == DYNA_MAGICKEY;
}
///////////////////////////////////////////////////////////////////////////////
struct dyna* dynaGetDyna(void* pObj)
{
	struct dyna* pDO = (struct dyna*)pObj - 1;

	if (!dynaIsDyna(pDO))
		return 0;

	return pDO;
}
///////////////////////////////////////////////////////////////////////////////
void* dynaCreate(int cbExtLen, dyna_callback* lpfnFree)
{
	struct dyna* pDO;

	int cbLen = sizeof(struct dyna) + cbExtLen;

	pDO = malloc(cbLen);
	if (!pDO)
		return 0;

	memset(pDO, 0, cbLen);

	pDO->dwMagicKey = DYNA_MAGICKEY;
	pDO->lpfnFree = 0;

	return dynaGetObj(pDO);
}

///////////////////////////////////////////////////////////////////////////////
int dynaGetLength(void* pObj)
{
	struct dyna* pDO = dynaGetDyna(pObj);

	return pDO->cbLen;
}
///////////////////////////////////////////////////////////////////////////////
void dynaFree(void* pObj)
{
	if (!pObj)
		return;

	struct dyna* pDO = dynaGetDyna(pObj);
	
	if (!pDO)
		return;

	if (pDO->lpfnFree)
		pDO->lpfnFree(pObj);

	dynaDefFree(pDO);
}

///////////////////////////////////////////////////////////////////////////////

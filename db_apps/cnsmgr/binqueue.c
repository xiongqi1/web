#include "binqueue.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
void binqueue_destroy(binqueue* pQ)
{
	__bypassIfNull(pQ);

	__free(pQ->pBuf);
	__free(pQ);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void binqueue_clear(binqueue* pQ)
{
	pQ->iH = pQ->iT = 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int binqueue_isEmpty(binqueue* pQ)
{
	return pQ->iH == pQ->iT;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void binqueue_waste(binqueue* pQ, int cbWaste)
{
	pQ->iH = (pQ->iH + cbWaste) % pQ->cbBuf;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int binqueue_peek(binqueue* pQ, void* pDst, int cbDst)
{
	char* pPtr = (char*)pDst;
	int iH = pQ->iH;

	while (pPtr<(char*)pDst+cbDst)
	{
		if (iH == pQ->iT)
			break;

		// copy
		*pPtr++ = pQ->pBuf[iH];

		// inc. hdr
		iH = (iH+1) % pQ->cbBuf;
	}

	return (int)(pPtr -(char*)pDst);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int binqueue_write(binqueue* pQ, void* pSrc, int cbSrc)
{
	char* pPtr = (char*)pSrc;
	int iNewT;

	while (pPtr<(char*)pSrc+cbSrc)
	{
		// get new T
		iNewT = (pQ->iT + 1) % pQ->cbBuf;
		if (iNewT == pQ->iH)
			break;

		// copy
		pQ->pBuf[pQ->iT] = *pPtr++;

		// inc. tail
		pQ->iT = iNewT;
	}

	return (int)(pPtr -(char*)pSrc);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
binqueue* binqueue_create(int cbLen)
{
	binqueue* pQ;

	// allocate obj
	__goToErrorIfFalse(pQ = __allocObj(binqueue));

	// allocate buffer
	pQ->cbBuf = cbLen + 1;
	__goToErrorIfFalse(pQ->pBuf = __alloc(pQ->cbBuf));

	return pQ;

error:
	return NULL;
}



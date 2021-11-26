#include <stdlib.h>
#include <string.h>

#include "atcmdqueue.h"


#define __min(x,y)					(((x)<(y) )?(x):(y))
#define __offset(p,offset)	((void*)(((char*)(p))+offset))


///////////////////////////////////////////////////////////////////////////////
int atcmdqueue_getlen(atcmdqueue* pQ)
{
	return pQ->cbMem - 1;
}
///////////////////////////////////////////////////////////////////////////////
int atcmdqueue_get_count(atcmdqueue* pQ)
{
	return (pQ->iT + pQ->cbMem - pQ->iH) % pQ->cbMem;
}

///////////////////////////////////////////////////////////////////////////////
int atcmdqueue_getfree(atcmdqueue* pQ)
{
	return pQ->cbMem - atcmdqueue_get_count(pQ) - 1;
}


//////////////////////////////////////////////////////////////////////////////
int atcmdqueue_seekEoC(atcmdqueue* pQ, const char* szLine)
{
	const char* pBuf = (const char*)pQ->pMem;
	int cbL = strlen(szLine);
	const char* pL = szLine;

	int iL = 0;
	int iH = pQ->iH;

	int cbPos = 0;

	iL = 1;
	while (iH != pQ->iT && iL < cbL)
	{
		cbPos++;

		if (pBuf[iH] != pL[iL])
			iL = 0;

		if (pBuf[iH] == pL[iL])
			iL++;

		iH = (iH + 1) % pQ->cbMem;
	}

	if (iL < cbL)
		return -1;

	return cbPos;
}

//////////////////////////////////////////////////////////////////////////////
int atcmdqueue_write(atcmdqueue* pQ, const void* pSrc, int cbSrc)
{
	int cbFreeLen;
	int cbRealSrc;

	void* p1stQ;
	int cb1stQ;

	void* p2ndBuf;
	int cb2ndQ;

	int cbWritten;

	// get free length
	cbFreeLen = atcmdqueue_getfree(pQ);

	// reduce cbSrc if less free space
	cbRealSrc = __min(cbSrc, cbFreeLen);

	// copy the 1st half
	p1stQ = __offset(pQ->pMem, pQ->iT);
	cb1stQ = __min(pQ->cbMem - pQ->iT, cbRealSrc);
	if (cb1stQ)
		memcpy(p1stQ, pSrc, cb1stQ);

	cbRealSrc -= cb1stQ;

	// copy the 2nd half
	p2ndBuf = __offset(pSrc, cb1stQ);
	cb2ndQ = __min(pQ->iH, cbRealSrc);
	if (cb2ndQ)
		memcpy(pQ->pMem, p2ndBuf, cb2ndQ);

	// advance the tail
	cbWritten = cb1stQ + cb2ndQ;
	pQ->iT = (pQ->iT + cbWritten) % pQ->cbMem;

	return cbWritten;
}


///////////////////////////////////////////////////////////////////////////////
void atcmdqueue_clear(atcmdqueue* pQ)
{
	pQ->iH = pQ->iT;
}
///////////////////////////////////////////////////////////////////////////////
int atcmdqueue_waste(atcmdqueue* pQ, int cbWaste)
{
	int cbQLen;
	int cbRealWaste;

	cbQLen = atcmdqueue_get_count(pQ);
	cbRealWaste = __min(cbQLen, cbWaste);

	pQ->iH = (pQ->iH + cbRealWaste) % pQ->cbMem;

	return cbRealWaste;
}

///////////////////////////////////////////////////////////////////////////////
int atcmdqueue_skip_peek(atcmdqueue* pQ, void* pDst, int cbDst, int cbSkip)
{
	int cbQLen;

	int cbRealDst;

	void* p1stQ;
	int cb1stQ;

	void* p2ndBuf;
	int cb2ndQ;

	int cbRead;

	int iH;

	if (atcmdqueue_get_count(pQ) <= cbSkip)
		return 0;

	iH = (pQ->iH + cbSkip) % (pQ->cbMem);

	// get length
	cbQLen = atcmdqueue_get_count(pQ);

	// reduce cbDst if less queue length
	cbRealDst = __min(cbDst, cbQLen);

	// copy the 1st half
	p1stQ = __offset(pQ->pMem, iH);
	cb1stQ = __min(pQ->cbMem - iH, cbRealDst);
	if (cb1stQ)
		memcpy(pDst, p1stQ, cb1stQ);

	cbRealDst -= cb1stQ;

	// copy the 2nd half
	p2ndBuf = __offset(pDst, cb1stQ);
	cb2ndQ = __min(pQ->iT, cbRealDst);
	if (cb2ndQ)
		memcpy(p2ndBuf, pQ->pMem, cb2ndQ);

	// advance the tail
	cbRead = cb1stQ + cb2ndQ;

	return cbRead;
}

///////////////////////////////////////////////////////////////////////////////
int atcmdqueue_peek(atcmdqueue* pQ, void* pDst, int cbDst)
{
	return atcmdqueue_skip_peek(pQ, pDst, cbDst, 0);
}
///////////////////////////////////////////////////////////////////////////////
int atcmdqueue_read(atcmdqueue* pQ, void* pDst, int cbDst)
{
	int cbRead = atcmdqueue_skip_peek(pQ, pDst, cbDst, 0);
	atcmdqueue_waste(pQ, cbRead);

	return cbRead;
}


///////////////////////////////////////////////////////////////////////////////
void atcmdqueue_destroy(atcmdqueue* pQ)
{
	if (!pQ)
		return ;

	if (!pQ->pMem)
		free(pQ->pMem);

	free(pQ);
}

///////////////////////////////////////////////////////////////////////////////
atcmdqueue* atcmdqueue_create(int cbMem)
{
	atcmdqueue* pQ;

	// allocate object
	pQ = malloc(sizeof(*pQ));
	if (!pQ)
		goto error;

	memset(pQ, 0, sizeof(*pQ));

	// allocate queue
	pQ->cbMem = cbMem;
	pQ->pMem = malloc(cbMem);
	if (!pQ->pMem)
		goto error;

	memset(pQ->pMem, 0, cbMem);

	return pQ;

error:
	atcmdqueue_destroy(pQ);
	return NULL;
}

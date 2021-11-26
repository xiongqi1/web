#include "utils.h"

#include <stdlib.h>
#include <ctype.h>
#include <sys/select.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h>
#include <sched.h>
#include <arpa/inet.h>

#include "owntypedef.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
long long __htonll(long long l64H)
{
	UINT64STRUC* pl64H = (UINT64STRUC*) & l64H;
	UINT64STRUC l64N;

	l64N.dw32lo = (DWORD)ntohl(pl64H->dw32hi);
	l64N.dw32hi = (DWORD)ntohl(pl64H->dw32lo);

	return l64N.u64;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
long long __ntohll(long long l64N)
{
	UINT64STRUC* pl64N = (UINT64STRUC*) & l64N;
	UINT64STRUC l64H;

	l64H.dw32lo = ntohl(pl64N->dw32hi);
	l64H.dw32hi = ntohl(pl64N->dw32lo);

	return l64H.u64;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
clock_t __getTicksPerSecond(void)
{
	return sysconf(_SC_CLK_TCK);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
clock_t __getTickCount(void)
{
	struct tms tm;

	return times(&tm);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int utils_strNLen(const char* pStr, int cbStr)
{
	int cbLen = 0;

	while (cbStr--)
	{
		if (*pStr++ == 0)
			break;

		cbLen++;
	}

	return cbLen;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utils_strTailTrim(char* szDst)
{
	int nStrLen = strlen(szDst);

	char* pTl = szDst + nStrLen - 1;
	char* pBreak = NULL;

	while (isspace(*pTl) && nStrLen--)
		pBreak = pTl--;

	if (pBreak)
		*pBreak = 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL utils_strTrimCpy(char* szDst, const char* szSrc, int cbSrc)
{
	int nStrLen = utils_strNLen(szSrc, cbSrc);
	const char* pHd = szSrc;
	const char* pTl = szSrc + nStrLen - 1;

	int cbCp;

	int cbLen;

	// get head
	cbLen = nStrLen;
	while (isspace(*pHd) && cbLen--)
		pHd++;

	// get tail
	cbLen = nStrLen;
	while (isspace(*pTl) && cbLen--)
		pTl--;

	if ((cbCp = (unsigned)(pTl - pHd) + 1) < 0)
		return FALSE;

	memcpy(szDst, pHd, cbCp);
	szDst[cbCp] = 0;

	return TRUE;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL utils_sleep(int nMS)
{
	if (nMS < 0)
	{
		__goToErrorIfFalse(__isSucc(pause()));
	}
	else
	{
		struct timespec tsReq;
		struct timespec tsRem;

		tsReq.tv_sec = nMS / 1000;
		tsReq.tv_nsec = (nMS % 1000) * 1000000;

		__goToErrorIfFalse(__isSucc(nanosleep(&tsReq, &tsRem)));
	}

	return TRUE;

error:
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void __free(void* pMem)
{
	if (pMem)
		free(pMem);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utils_strConv2Bytes(char* lpszDest, const char* p2ByteSrc, int c2ByteSrc)
{
	unsigned char bCode;

	p2ByteSrc++;

	while (c2ByteSrc--)
	{
		bCode = *lpszDest++ = *p2ByteSrc;
		p2ByteSrc += 2;

		if (!bCode)
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utils_strSNCpy(char* lpszDst, const char* lpszSrc, int cbSrc)
{
	strncpy(lpszDst, lpszSrc, cbSrc);
	lpszDst[cbSrc] = 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utils_strDNCpy(char* lpszDst, int cbDst, const char* lpszSrc)
{
	strncpy(lpszDst, lpszSrc, cbDst);
	lpszDst[cbDst-1] = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* __alloc(int cbLen)
{
	void* pMem = malloc(cbLen+1);

	if (pMem)
		__zeroMem(pMem, cbLen+1);

	return pMem;
}

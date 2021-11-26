#ifndef __SMARTQUEUE_H_
#define __SMARTQUEUE_H_

///////////////////////////////////////////////////////////////////////////////
typedef struct
{
	void* pMem;
	int cbMem;

	int iH;
	int iT;
} atcmdqueue;


///////////////////////////////////////////////////////////////////////////////

int atcmdqueue_write(atcmdqueue* pQ, const void* pSrc, int cbSrc);
int atcmdqueue_waste(atcmdqueue* pQ, int cbWaste);
void atcmdqueue_clear(atcmdqueue* pQ);
int atcmdqueue_peek(atcmdqueue* pQ, void* pDst, int cbDst);
int atcmdqueue_skip_peek(atcmdqueue* pQ, void* pDst, int cbDst, int cbSkip);
void atcmdqueue_destroy(atcmdqueue* pQ);
atcmdqueue* atcmdqueue_create(int cbMem);
int atcmdqueue_get_count(atcmdqueue* pQ);
int atcmdqueue_getfree(atcmdqueue* pQ);
int atcmdqueue_getlen(atcmdqueue* pQ);
int atcmdqueue_seekEoC(atcmdqueue* pQ, const char* szLine);
int atcmdqueue_read(atcmdqueue* pQ, void* pDst, int cbDst);

#endif

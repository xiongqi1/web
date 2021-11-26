#ifndef __BINQUEUE_H__
#define __BINQUEUE_H__

#include "base.h"

typedef struct
{
	int iH;
	int iT;

	char* pBuf;
	int cbBuf;

} binqueue;


binqueue* binqueue_create(int cbLen);
void binqueue_destroy(binqueue* pQ);

void binqueue_clear(binqueue* pQ);
int binqueue_isEmpty(binqueue* pQ);

int binqueue_peek(binqueue* pQ, void* pDst, int cbDst);
void binqueue_waste(binqueue* pQ, int cbWaste);
int binqueue_write(binqueue* pQ, void* pSrc, int cbSrc);

#endif

#ifndef __DYNA_H__
#define __DYNA_H__

///////////////////////////////////////////////////////////////////////////////
typedef void (dyna_callback)(void* pRef);

///////////////////////////////////////////////////////////////////////////////
struct dyna 
{
	unsigned int dwMagicKey;
	int cbLen;

	dyna_callback* lpfnFree;
};


void* dynaCreate(int cbExtLen,dyna_callback* lpfnFree);
void dynaFree(void* pObj);

int dynaGetLength(void* pObj);

#endif

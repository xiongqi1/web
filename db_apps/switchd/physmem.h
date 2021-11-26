#ifndef __PHYSMEM_H__
#define __PHYSMEM_H__

#include <stdlib.h>

typedef struct 
{
	int hMem;
	
	void* pMem;
	size_t cbMem;

	void* pPtr;
} physmem;


void physmem_destroy(physmem* pP);
physmem* physmem_create(__off_t lPhysMem,size_t cbLen);
void* physmem_getPtr(physmem* pP);

#endif

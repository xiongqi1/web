#include "physmem.h"

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
void* physmem_getPtr(physmem* pP)
{
	return pP->pPtr;
}
///////////////////////////////////////////////////////////////////////////////
void physmem_destroy(physmem* pP)
{
	if(!pP)
		return;

	if(pP->pMem)
		munmap(pP->pMem,pP->cbMem);

	if(!(pP->hMem<0))
		close(pP->hMem);
}

///////////////////////////////////////////////////////////////////////////////
physmem* physmem_create(__off_t lPhysMem,size_t cbLen)
{
	physmem* pP;

	// allocate for object
	pP=malloc(sizeof(*pP));
	if(!pP)
		goto error;
	memset(pP,0,sizeof(*pP));

	pP->hMem=-1;
	pP->pMem=(void*)-1;

	// open mem
	pP->hMem = open("/dev/mem", O_RDWR | O_SYNC);
	if(pP->hMem<0)
		goto error;

	// get page len and address
	long int cbPgSize=sysconf(_SC_PAGE_SIZE);
	long int nPgMask=cbPgSize-1;
	__off_t lMemS=lPhysMem & ~nPgMask;
	__off_t lMemE=(lPhysMem+cbLen+nPgMask) & ~nPgMask;
	__off_t lOff=lPhysMem-lMemS;
	size_t lMemLen=lMemE-lMemS;

	// map phys into process memory
	pP->pMem=mmap(0,lMemLen,PROT_READ|PROT_WRITE,MAP_SHARED,pP->hMem,lMemS);
	if(pP->pMem == (void*)-1)
		goto error;

	pP->pPtr=(void*)( (size_t)pP->pMem+(size_t)lOff );

	return pP;

error:
	physmem_destroy(pP);
	return NULL;
}

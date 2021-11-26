#ifndef __GROWINGMEM_H__
#define __GROWINGMEM_H__

#include "base.h"


typedef struct
{
	void* pMem;
	int cbMem;

	int cbPage;

} pagedmem;


pagedmem* growingmem_create(int cbPage);
BOOL growingmem_growIfNeeded(pagedmem* pPaged, int cbContent);
void* growingmem_getMem(pagedmem* pPaged);
void growingmem_destroy(pagedmem* pPaged);

#endif

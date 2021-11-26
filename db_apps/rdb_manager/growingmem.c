#include "growingmem.h"

#include <stdlib.h>
#include <string.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void growingmem_destroy(pagedmem* pPaged)
{
	if (!pPaged)
		return;

	if (pPaged->pMem)
		free(pPaged->pMem);

	free(pPaged);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* growingmem_getMem(pagedmem* pPaged)
{
	return pPaged->pMem;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL growingmem_growIfNeeded(pagedmem* pPaged, int cbContent)
{
	// bypass if already capable
	if (cbContent <= pPaged->cbMem)
		return TRUE;

	int cbNewMem = ((cbContent + pPaged->cbPage - 1) / pPaged->cbPage) * pPaged->cbPage;

	// free
	if (pPaged->pMem)
		free(pPaged->pMem);
	pPaged->cbMem = 0;

	// alloc
	if (cbNewMem && NULL != (pPaged->pMem = malloc(cbNewMem)))
	{
		memset(pPaged->pMem, 0, cbNewMem);
		pPaged->cbMem = cbNewMem;
	}

	return pPaged->pMem != NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
pagedmem* growingmem_create(int cbPage)
{
	pagedmem* pPaged = NULL;

	if (!(pPaged = malloc(sizeof(pagedmem))))
		goto error;

	memset(pPaged, 0, sizeof(*pPaged));

	pPaged->cbPage = cbPage;

	return pPaged;
error:
	growingmem_destroy(pPaged);
	return NULL;
}

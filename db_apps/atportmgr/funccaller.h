#ifndef __FUNCCALLER_H__
#define __FUNCCALLER_H__

#include <stdlib.h>
#include "list.h"
#include "tickcount.h"
#include "moduledatabase.h"

////////////////////////////////////////////////////////////////////////////////
struct callback_entry_t;
struct funccaller_t;

////////////////////////////////////////////////////////////////////////////////
typedef int (funccaller_callback)(struct funccaller_t* pF, struct callback_entry_t* pEnt);

////////////////////////////////////////////////////////////////////////////////
typedef struct callback_entry_t
{
	struct list_head listHdr;								// list header

	funccaller_callback* pCallbackQuery;		// query callback
	funccaller_callback* pCallbackAns;			// answer callback

	callback_entry_type entType;						// entry type

	tick tickQuery;													// tick when query
	const void* pRef;												// user reference

} callback_entry;

////////////////////////////////////////////////////////////////////////////////
typedef struct funccaller_t
{
	struct list_head listHdr;			// callbacck entry header
	callback_entry* pCurEnt;			// current callback entry
	tick tckTimeOutMS;						// timeout
} funccaller;

funccaller* funccaller_create(tick tckTimeOutMS);
callback_entry* funccaller_append(funccaller* pF, funccaller_callback* pCallbackQuery, funccaller_callback* pCallbackAns, callback_entry_type entType, const void* pRef);
callback_entry* funccaller_insert(funccaller* pF, funccaller_callback* pCallbackQuery, funccaller_callback* pCallbackAns, callback_entry_type entType, const void* pRef);
callback_entry* funccaller_insert_before(funccaller* pF, funccaller_callback* pCallbackQuery, funccaller_callback* pCallbackAns, callback_entry_type entType, const void* pRef, const callback_entry* pEnt);
callback_entry* funccaller_moveOn(funccaller* pF, int fSuccess, int* pWrapped);
callback_entry* funccaller_getEntry(funccaller* pF);
void funccaller_destroy(funccaller* pF);

int funccaller_callAnsCallback(funccaller* pF, callback_entry* pE);
int funccaller_callQueryCallback(funccaller* pF, callback_entry* pE, tick tckCur);
int funccaller_isTimeOut(funccaller* pF, callback_entry* pE, tick tckCur);
void funccaller_setTimeOut(funccaller* pF, callback_entry* pE, tick tckCur);

#endif

/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "list.h"
#include "utils.h"
#include "globals.h"

static int insertEntry( List *, void * );

void initList( List *list )
{
	list->firstEntry = NULL;
	list->lastEntry  = NULL;
}

ListEntry *getFirstEntry( List *list )
{
	return list->firstEntry;
}

ListEntry *getLastEntry( List *list )
{
	return list->lastEntry;
}

int getListSize( List *list )
{
	int cnt = 0;
	ListEntry *tmp = getFirstEntry( list );
	while( tmp ) {
		cnt ++;
		tmp = tmp->next;
	}
	return cnt;
}

/** Removes the first entry from the list and returns the data pointer
	call repeatly until a NULL pointer returns.
*/
void *freeList(List *list)
{
	void *ret;
	ListEntry *el = list->firstEntry;
	if ( el != NULL ) {	
		list->firstEntry = el->next;
		ret = el->data;	
		efree( el );
	} else {
		// reset the lastEntry if the list is empty 
		list->lastEntry = el;
		ret = NULL;
	}
	return ret;
}

int addEntry( List *list, void *object )
{
	int ret = LIST_OK;
	
	ret = insertEntry( list, object );
	return ret;
}

/** Finds an entry in the list, uses the comp() to compare the parameter from
    with the data from a list entry.
	The comp() has to return 0 if the data is equal
	If nothing is found NULL is returned.
*/
void *findEntry( List *list, void *object, int (*comp)( void *, void *))
{
	ListEntry *el = list->firstEntry;
	while( el != NULL ) {
		if ( comp( object, el->data ) == 0 )
			return el->data;
		el = el->next;
	}
	return NULL;
}

/** Add an entry if the data is not already in the list
	see findEntry()

	@ret 	0 	already in the list
			1 	inserted 
*/
int addEntryUniq( List *list, void *object, int (*comp)( void *, void *))
{
	int ret = LIST_OK;
	
	if ( findEntry( list, object, comp ) != NULL )
		return LIST_OK;
	
	ret = insertEntry( list, object );
	return ret;
}

static int insertEntry( List *list, void *object )
{
	ListEntry *el;

	el = (ListEntry*)emalloc( sizeof( ListEntry ));
    if ( el == NULL )
        return ERR_RESOURCE_EXCEED;

	if ( list->firstEntry == NULL )
		list->firstEntry = el;

	el->data = object;
	el->next = NULL;
	if ( list->lastEntry != NULL )
		list->lastEntry->next = el;
	list->lastEntry = el;
    return LIST_OK;
}

void *removeEntry( List *list, void *object, int (*comp)( void *, void *))
{
	void *ret = NULL;
	ListEntry *tmp = list->firstEntry;
	ListEntry *el = list->firstEntry;

	while( el != NULL ) {
		if ( comp( object, el->data ) == 0 ) {
			// only one entry in the list
			if ( el == list->firstEntry && el == list->lastEntry ) {
				list->firstEntry = list->lastEntry = NULL;
				ret = el->data;
				efree( el );
				return ret;
			}	
			if ( el == list->firstEntry ) {
				list->firstEntry = el->next;
				ret = el->data;
				efree( el );
				return ret;
			}
			// remove el from tmp
			tmp->next = el->next;
			ret = el->data;
			efree( el );
			return ret;
		}
		tmp = el;
		el = el->next;
	}

	return NULL;
}

/** Iterates over the given list
 * Start the iteration with the entry value NULL.
 * 
 * \param	List*		Ptr to the list to iterate
 * \param 	ListEntry*	Ptr to the actual listEntry or NULL to start iteration
 * 
 * \return ListEntry*	the next list entry of parameter entry or NULL
 * 						if list is empty
 */
ListEntry *iterateList( List *list, ListEntry *entry )
{
	if ( entry == NULL )
		return list->firstEntry;
	else
		return entry->next;
}

/** Iterates over the given list, removes the given entry from the list
 * and returns the next ListEntry from the list.
 * The ListEntry which should be removed from the list, must have freed it's user data already
 * otherwise there is a memory leak.
 * 
 * \param	List*		Ptr to the list to iterate
 * \param 	ListEntry*	Ptr to the removable listEntry 
 * 
 * \return ListEntry*	the next list entry of parameter entry or NULL
 * 						if list is empty
 */

ListEntry *iterateRemove( List *list, ListEntry *entry )
{
	ListEntry *tmp = list->firstEntry;
	ListEntry *el = list->firstEntry;

	while( el != NULL ) {
			if ( entry ==  el ) {
					// only one entry in the list
					if ( el == list->firstEntry && el == list->lastEntry ) {
						list->firstEntry = list->lastEntry = NULL;
						efree( el );
						return NULL;
					}
					if ( el == list->firstEntry ) {
						list->firstEntry = el->next;
						efree( el );
						return list->firstEntry;
							
					}

					//for memory leak
					if (el == list->lastEntry) {
						list->lastEntry = tmp;
					}

					// remove el from tmp
					tmp->next = el->next;
					efree( el );
					return tmp->next;
			}
			tmp = el;
			el = el->next;
	}

	return NULL;
}

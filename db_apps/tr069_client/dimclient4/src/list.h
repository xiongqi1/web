/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef list_H
#define list_H

#include <stdio.h>

#include "globals.h"

#define LIST_OK 				0
#define LIST_ITEM_FOUND			1

typedef struct listentry {
		void			 *data;
		struct listentry *next;
} ListEntry;
	
typedef struct list {
	ListEntry *firstEntry;
	ListEntry *lastEntry;
} List;
	
/** Removes the first entry from the list and returns the data pointer
	call repeatly until a NULL pointer returns.
*/
void *freeList (List *);
ListEntry *getFirstEntry( List * );
ListEntry *getLastEntry( List *list );
void initList( List * );
int getListSize( List * );
int addEntry( List *, void * );
void *findEntry( List *, void *, int (*comp)( void *, void *));
int addEntryUniq( List *, void *, int (*comp)( void *, void *));
void *removeEntry( List *, void *, int (*comp)( void *, void *));
ListEntry *iterateList( List *, ListEntry * );
ListEntry *iterateRemove( List *, ListEntry * );

#endif /* list_H */

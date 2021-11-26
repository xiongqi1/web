#include <stdio.h>

#include "linkedlist.h"

void linkedlist_init(struct linkedlist* list)
{
	list->next=list->prev=NULL;
}

void linkedlist_del(struct linkedlist* list)
{
	struct linkedlist* next;
	struct linkedlist* prev;

	next=list->next;
	prev=list->prev;

	if(prev)
		prev->next=next;

	if(next)
		next->prev=prev;
}

struct linkedlist* linkedlist_addex(struct linkedlist* list,struct linkedlist* new,int dir)
{
	struct linkedlist* next;
	struct linkedlist* prev;

	if(dir) {
		prev=list;
		next=list->next;
	}
	else {
		prev=list->prev;
		next=list;
	}

	new->prev=prev;
	new->next=next;

	if(prev)
		prev->next=new;

	if(next)
		next->prev=new;

	return new;
}

struct linkedlist* linkedlist_insert(struct linkedlist* list,struct linkedlist* new)
{
	return linkedlist_addex(list,new,0);
}

struct linkedlist* linkedlist_add(struct linkedlist* list,struct linkedlist* new)
{
	return linkedlist_addex(list,new,1);
}

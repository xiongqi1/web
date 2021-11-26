#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__

struct linkedlist;

struct linkedlist {
	struct linkedlist* prev;
	struct linkedlist* next;
};

void linkedlist_init(struct linkedlist* list);
void linkedlist_del(struct linkedlist* list);
struct linkedlist* linkedlist_add(struct linkedlist* list,struct linkedlist* new);
struct linkedlist* linkedlist_insert(struct linkedlist* list,struct linkedlist* new);

#endif

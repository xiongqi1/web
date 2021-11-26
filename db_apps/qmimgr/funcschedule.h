#ifndef __FUNCSCHEDULE_H__
#define __FUNCSCHEDULE_H__

#define FUNCSCHEDULE_MAX_ELEMENT	100

#include <sys/times.h>

#include "linkedlist.h"

struct funcschedule_element_t;

typedef void (*funcschedule_callback)(struct funcschedule_element_t* element);

struct funcschedule_element_t {

	struct linkedlist list;

	clock_t start;

	unsigned long delay;
	funcschedule_callback callback;
	void* ref;

	unsigned long long key;
};

struct funcschedule_t {
	struct funcschedule_element_t elements[FUNCSCHEDULE_MAX_ELEMENT];

	struct linkedlist running_root;
	struct linkedlist* running_last;

	struct linkedlist idle_root;

	struct generictree_t* keytree;

	int suspend;
};

void funcschedule_exec(struct funcschedule_t* schedule);
int funcschedule_cancel(struct funcschedule_t* schedule, unsigned long long key);
struct funcschedule_t* funcschedule_create();
void funcschedule_destroy(struct funcschedule_t* schedule);
int funcschedule_add(struct funcschedule_t* schedule,unsigned long long key,unsigned long delay,funcschedule_callback callback,void* ref);

void funcschedule_resume(struct funcschedule_t* schedule);
void funcschedule_suspend(struct funcschedule_t* schedule);

#endif

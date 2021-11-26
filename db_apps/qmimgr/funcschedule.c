
#include <unistd.h>

#include "funcschedule.h"
#include "minilib.h"
#include "generictree.h"

static void funcschedule_move_to_running(struct funcschedule_t* schedule,struct funcschedule_element_t* element)
{
	linkedlist_del(&element->list);
	linkedlist_add(schedule->running_last,&element->list);
}

static void funcschedule_remove_from_running(struct funcschedule_t* schedule,struct funcschedule_element_t* element)
{
	// remove it from running
	if(&element->list==schedule->running_last)
		schedule->running_last=element->list.prev;
	linkedlist_del(&element->list);
}

static void funcschedule_insert_to_idle(struct funcschedule_t* schedule,struct funcschedule_element_t* element)
{
	// add it to idle
	linkedlist_add(&schedule->idle_root,&element->list);
}

void funcschedule_suspend(struct funcschedule_t* schedule)
{
	schedule->suspend++;

	if(schedule->suspend<=0) {
		SYSLOG(LOG_ERROR,"internal suspend count error!!");
	}
}

void funcschedule_resume(struct funcschedule_t* schedule)
{
	schedule->suspend--;

	if(schedule->suspend<0) {
		SYSLOG(LOG_ERROR,"internal resume count error!!");
	}
}

void funcschedule_exec(struct funcschedule_t* schedule)
{
	struct funcschedule_element_t* element;
	struct funcschedule_element_t* next;
	clock_t now;

	if(!schedule->suspend) {
		now=_get_current_sec();

		next=(struct funcschedule_element_t*)schedule->running_root.next;
		while((element=next)!=0) {
			next=(struct funcschedule_element_t*)element->list.next;
			if( now-element->start < element->delay )
				continue;

			funcschedule_remove_from_running(schedule,element);

			if(element->key) {
				if( generictree_del(schedule->keytree,element->key)<0 ) {
					SYSLOG(LOG_ERROR,"key not found in keytree - %lld",element->key);
				}
			}

			element->callback(element);

			funcschedule_insert_to_idle(schedule,element);

		}
	}
}

int funcschedule_cancel(struct funcschedule_t* schedule, unsigned long long key)
{
	struct funcschedule_element_t* element;

	element=(struct funcschedule_element_t*)generictree_find(schedule->keytree,key);
	if(!element) {
		SYSLOG(LOG_OPERATION,"key not found in keytree - key=%lld",key);
		goto err;
	}

	funcschedule_remove_from_running(schedule,element);
	funcschedule_insert_to_idle(schedule,element);

	generictree_del(schedule->keytree,key);

	return 0;

err:
	return -1;
}

int funcschedule_add(struct funcschedule_t* schedule,unsigned long long key,unsigned long delay,funcschedule_callback callback,void* ref)
{
	clock_t now;

	struct funcschedule_element_t* element;

	now=_get_current_sec();

	// check parameter
	if(!callback) {
		SYSLOG(LOG_ERROR,"callback is null");
		goto err;
	}

	// get first idle_root
	element=(struct funcschedule_element_t*)schedule->idle_root.next;
	if(!element) {
		SYSLOG(LOG_ERROR,"out of schedule slot - max=%d",FUNCSCHEDULE_MAX_ELEMENT);
		goto err;
	}

	// add running
	element->key=key;
	element->delay=delay;
	element->callback=callback;
	element->ref=ref;
	element->start=now;

	funcschedule_move_to_running(schedule,element);

	// register in keytree
	if(key) {
		if(generictree_add(schedule->keytree,key,element)<0)
			goto err;
	}

	return 0;

err:
	return -1;
}

void funcschedule_destroy(struct funcschedule_t* schedule)
{
	if(!schedule)
		return;

	generictree_destroy(schedule->keytree);

	_free(schedule);
}

struct funcschedule_t* funcschedule_create(void)
{
	struct funcschedule_t* schedule;
	int i;

	// create the object
	schedule=(struct funcschedule_t*)_malloc(sizeof(struct funcschedule_t));
	if(!schedule) {
		SYSLOG(LOG_ERROR,"failed to allocate funcschedule_t - size=%d",sizeof(struct funcschedule_t));
		goto err;
	}

	// create keytree
	schedule->keytree=generictree_create();
	if(!schedule->keytree)
		goto err;

	// build idle list
	linkedlist_init(&schedule->idle_root);
	for(i=0;i<FUNCSCHEDULE_MAX_ELEMENT;i++)
		linkedlist_add(&schedule->idle_root,&schedule->elements[i].list);

	// build running list
	linkedlist_init(&schedule->running_root);
	schedule->running_last=&schedule->running_root;

	return schedule;

err:
	funcschedule_destroy(schedule);

	return NULL;

}



#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "strqueue.h"

#include "minilib.h"

void strqueue_debug_dump(struct strqueue_t* q)
{
	int i;
	struct strqueue_element_t* el;

	SYSLOG(LOG_DEBUG,"strqueue - 0x%p",q);

	SYSLOG(LOG_DEBUG,"total_elements = %d",q->total_elements);
	SYSLOG(LOG_DEBUG,"active_count = %d",q->active_count);

	// get active list
	i=0;
	el=(struct strqueue_element_t*)q->active.list.next;
	while(el) {
		SYSLOG(LOG_DEBUG,"active list idx=%d, addr=0x%p",i,el);
		el=(struct strqueue_element_t*)el->list.next;
		i++;
	}
	SYSLOG(LOG_DEBUG,"total active list = %d",i);

	// get inactive list
	i=0;
	el=(struct strqueue_element_t*)q->inactive.list.next;
	while(el) {
		SYSLOG(LOG_DEBUG,"inactive list idx=%d, addr=0x%p",i,el);
		el=(struct strqueue_element_t*)q->inactive.list.next;
		i++;
	}
	SYSLOG(LOG_DEBUG,"total inactive list = %d",i);
}

int strqueue_is_empty(struct strqueue_t* q)
{
	return !q->active.list.next;
}

struct strqueue_element_t* strqueue_walk_next(struct strqueue_t* q)
{
	if(q->walk)
		q->walk=(struct strqueue_element_t*)q->walk->list.next;

	return q->walk;
}

struct strqueue_element_t* strqueue_walk_to(struct strqueue_t* q,int count)
{
	int i;
	struct strqueue_element_t* el;

	i=0;
	el=strqueue_walk_first(q);
	while(el && (i++<count))
		el=strqueue_walk_next(q);

	return el;
}

struct strqueue_element_t* strqueue_walk_first(struct strqueue_t* q)
{
	q->walk=(struct strqueue_element_t*)&q->active;

	return strqueue_walk_next(q);
}

static struct strqueue_element_t* strqueue_create_inactive_element(struct strqueue_t* q)
{
	struct strqueue_element_t* el;

	// check boundary
	if(q->total_elements>=STRQUEUE_MAX_STR) {
		SYSLOG(LOG_ERROR,"reached maximum element - STRQUEUE_MAX_STR=%d",STRQUEUE_MAX_STR);
		goto err;
	}

	// create new element
	el=(struct strqueue_element_t*)_malloc(sizeof(*el));
	if(!el) {
		SYSLOG(LOG_ERROR,"failed to allocate elmenet - len=%d",sizeof(*el));
		goto err;
	}

	// add into inactive list
	q->total_elements++;
	linkedlist_add(&q->inactive.list,&el->list);

	return el;
err:
	return NULL;
}

static void strqueue_add_to_active(struct strqueue_t* q,struct strqueue_element_t* el)
{
	// remove el from inactive
	linkedlist_del(&el->list);
	if(el->count)
		(*el->count)--;

	// insert to active
	linkedlist_add(&q->active_tail->list,&el->list);
	el->count=&q->active_count;
	(*el->count)++;

	// update trail
	q->active_tail=el;
}

static void strqueue_add_to_inactive(struct strqueue_t* q,struct strqueue_element_t* el)
{
	// remove el from inactive
	linkedlist_del(&el->list);
	if(el->count)
		(*el->count)--;

	// insert to inactive
	linkedlist_add(&q->inactive.list,&el->list);
	el->count=NULL;
}


int strqueue_vomit(struct strqueue_t* q, struct strqueue_t* dst)
{
	struct strqueue_element_t* el;
	struct strqueue_element_t* next;
	int i;

	el=strqueue_walk_first(q);

	i=0;
	while(el) {
		next=(struct strqueue_element_t*)q->walk->list.next;
		if(!next)
			q->active_tail=(struct strqueue_element_t*)el->list.prev;

		strqueue_add_to_active(dst,el);
		el=strqueue_walk_first(q);

		i++;
	}

	q->total_elements-=i;
	dst+=i;

	SYSLOG(LOG_DEBUG,"vomit=%d,total=%d",i,q->total_elements);

	return i;
}

int strqueue_eat(struct strqueue_t* q, struct strqueue_t* dst)
{
	struct strqueue_element_t* el;
	struct strqueue_element_t* next;
	int i;

	el=strqueue_walk_first(dst);

	i=0;
	while(el) {
		next=(struct strqueue_element_t*)el->list.next;
		// update tail
		if(!next)
			dst->active_tail=(struct strqueue_element_t*)el->list.prev;

		strqueue_add_to_inactive(q,el);
		el=strqueue_walk_first(dst);

		i++;
	}

	q->total_elements+=i;
	dst-=i;

	SYSLOG(LOG_DEBUG,"eat=%d,total=%d",i,q->total_elements);


	return i;
}

int strqueue_remove(struct strqueue_t* q,int count)
{
	int i;
	struct strqueue_element_t* el;
	struct strqueue_element_t* next;

	el=(struct strqueue_element_t*)q->active.list.next;

	i=0;
	while(el && i<count) {
		next=(struct strqueue_element_t*)el->list.next;

		// update tail
		if(!next)
			q->active_tail=(struct strqueue_element_t*)el->list.prev;

		strqueue_add_to_inactive(q,el);

		el=next;

		i++;
	}

	return i;
}

const char* strqueue_add(struct strqueue_t* q, const struct strqueue_element_t* new_el)
{
	struct strqueue_element_t* el;

	// get inactive elmenet
	el=(struct strqueue_element_t*)q->inactive.list.next;
	if(!el) {
		el=strqueue_create_inactive_element(q);
		if(!el) {
			SYSLOG(LOG_ERROR,"failed to create inactive element");
			goto err;
		}
	}

	// resize if need
	int new_str_len;
	int new_alloc;

	new_str_len=strlen(new_el->str)+1;
	new_alloc=((new_str_len+STRQUEUE_STR_BLOCK-1)/STRQUEUE_STR_BLOCK)*STRQUEUE_STR_BLOCK;

	if(new_alloc>STRQUEUE_MAX_STR_LEN) {
		SYSLOG(LOG_ERROR,"too big str detected - %d",new_alloc);
		goto err;
	}

	if(new_alloc>el->alloc) {
		// free previous
		_free(el->str);
		el->str=NULL;

		// alloc new
		el->str=_malloc(new_alloc);
		if(!el->str) {
			SYSLOG(LOG_ERROR,"failed to resize element - len=%d",new_alloc);
			goto err;
		}

		el->alloc=new_alloc;
	}

	__strncpy(el->str,new_el->str,new_str_len);
	el->tran_id=new_el->tran_id;
	el->timeout=new_el->timeout;

	strqueue_add_to_active(q,el);

	SYSLOG(LOG_DEBUG,"added");
	return el->str;

err:
	return NULL;
}

void strqueue_destroy(struct strqueue_t* q)
{
	struct strqueue_element_t* el;
	struct strqueue_element_t* next;

	if(!q)
		return;

	// free active
	el=(struct strqueue_element_t*)q->active.list.next;
	while(el) {
		next=(struct strqueue_element_t*)el->list.next;
		free(el);
		el=next;
	}

	// free inactive
	el=(struct strqueue_element_t*)q->inactive.list.next;
	while(el) {
		next=(struct strqueue_element_t*)el->list.next;
		free(el);
		el=next;
	}

	_free(q);
}

struct strqueue_t* strqueue_create()
{
	struct strqueue_t* q;

	// create the object
	q=(struct strqueue_t*)_malloc(sizeof(struct strqueue_t));
	if(!q) {
		SYSLOG(LOG_ERROR,"failed to allocate strqueue_t - size=%d",sizeof(struct strqueue_t));
		goto err;
	}

	linkedlist_init(&q->active.list);
	linkedlist_init(&q->inactive.list);

	q->active_tail=(struct strqueue_element_t*)&q->active.list;

	return q;

err:
	strqueue_destroy(q);

	return NULL;

}

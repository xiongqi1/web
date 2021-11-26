#include <sglib.h>

#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "plmn_struc.h"

/*
	common types
*/

struct generic_t {
	struct plmn_t* plmn;
	struct plmn_t builtin_plmn;

	char color_field;

	struct generic_t* left;
	struct generic_t* right;

	struct generic_t* next;
};

typedef struct generic_t rbtplmn;
typedef struct generic_t llplmn;

// rbt
#define RBTPLMN_CMPARATOR(x,y) (MCCMNC((x)->plmn->mcc,(x)->plmn->mnc)-MCCMNC((y)->plmn->mcc,(y)->plmn->mnc) )
SGLIB_DEFINE_RBTREE_PROTOTYPES(rbtplmn, left, right, color_field, RBTPLMN_CMPARATOR)
SGLIB_DEFINE_RBTREE_FUNCTIONS(rbtplmn, left, right, color_field, RBTPLMN_CMPARATOR)

// define rbtplmn
struct rbtplmn_t {
	rbtplmn* rbt_root;

	struct sglib_rbtplmn_iterator it_rbt;
};

// ll
//#define LLPLMN_CMPARATOR(x,y) ((x)->plmn->rank-(y)->plmn->rank)
//To support cost effective mode, need to be sorted with act, as well.
#define LLPLMN_CMPARATOR(x,y) (MCCMNC((x)->plmn->rank,(y)->plmn->act)-MCCMNC((y)->plmn->rank,(x)->plmn->act))
SGLIB_DEFINE_LIST_PROTOTYPES(llplmn, LLPLMN_CMPARATOR, next)
SGLIB_DEFINE_LIST_FUNCTIONS(llplmn, LLPLMN_CMPARATOR, next)

struct llplmn_t {
	llplmn* ll_root;

	struct sglib_llplmn_iterator it_ll;
};


/*
	rbt functions
*/

struct plmn_t* rbtplmn_find(struct rbtplmn_t* o,struct plmn_t* plmn)
{
	rbtplmn* rbt;
	rbtplmn e;

	e.plmn=plmn;
	if( !(rbt=sglib_rbtplmn_find_member(o->rbt_root,&e)) )
		return 0;

	return rbt->plmn;
}

int rbtplmn_del(struct rbtplmn_t* o,struct plmn_t* plmn)
{
	rbtplmn* rbt;
	rbtplmn e;

	// search rbt
	e.plmn=plmn;
	if( !(rbt=sglib_rbtplmn_find_member(o->rbt_root,&e)) ) {
		syslog(LOG_ERR,"rbt not found - mcc=%d,mnc=%d",plmn->mcc,plmn->mnc);
		goto err;
	}

	// remove rbt from the rbtplmn
	sglib_rbtplmn_delete(&o->rbt_root,rbt);
	// destroy rbt
	free(rbt);

	return 0;

err:
	return -1;
}

struct plmn_t* rbtplmn_add(struct rbtplmn_t* o,struct plmn_t* plmn,int copy)
{
	rbtplmn* rbt;
	rbtplmn e;

	// find rbt or create one
	e.plmn=plmn;
	if( (rbt=sglib_rbtplmn_find_member(o->rbt_root,&e))!=0 ) {
		syslog(LOG_ERR,"rbt already exists - mcc=%d,mnc=%d",plmn->mcc,plmn->mnc);
		goto err;
	}

	if(!(rbt=malloc(sizeof(*rbt)))) {
		syslog(LOG_ERR,"rbt memory allocation failure - %s",strerror(errno));
		goto err;
	}

	// init rbt
	memset(rbt,0,sizeof(*rbt));
	if(copy) {
		rbt->plmn=&(rbt->builtin_plmn);
		memcpy(rbt->plmn,plmn,sizeof(*plmn));
	}
	else {
		rbt->plmn=plmn;
	}

	sglib_rbtplmn_add(&o->rbt_root,rbt);

	return rbt->plmn;

err:
	return 0;
}

void rbtplmn_destroy(struct rbtplmn_t* o)
{
	struct sglib_rbtplmn_iterator it;
	rbtplmn* rbt;


	for(rbt=sglib_rbtplmn_it_init(&it,o->rbt_root); rbt!=NULL; rbt=sglib_rbtplmn_it_next(&it)) {
		// destroy rbtt
		free(rbt);
	}

	free(o);
}

struct rbtplmn_t* rbtplmn_create()
{
	struct rbtplmn_t* o=NULL;

	// allocate memory
	o=malloc(sizeof(*o));
	if(!o) {
		syslog(LOG_ERR,"rbt object memory allocation failure - %s",strerror(errno));
		goto err;
	}

	// reset memory
	memset(o,0,sizeof(*o));

	return o;

err:
	if(o)
		free(o);

	return NULL;
}

struct plmn_t* rbtplmn_get_next(struct rbtplmn_t* o)
{
	rbtplmn* rbt;

	// init rbt iterator
	if(!(rbt=sglib_rbtplmn_it_next(&o->it_rbt)))
		return NULL;

	return rbt->plmn;
}

struct plmn_t* rbtplmn_get_first(struct rbtplmn_t* o)
{
	rbtplmn* rbt;

	// init rbt iterator
	if(!(rbt=sglib_rbtplmn_it_init(&o->it_rbt,o->rbt_root)))
		return NULL;

	return rbt->plmn;
}

/*
	ll functions
*/


struct plmn_t* llplmn_find(struct llplmn_t* o,struct plmn_t* plmn)
{
	static llplmn* ll;
	llplmn e;

	e.plmn=plmn;
	if( !(ll=sglib_llplmn_find_member(o->ll_root,&e)) )
		return 0;

	return ll->plmn;
}

int llplmn_del(struct llplmn_t* o,struct plmn_t* plmn)
{
	llplmn* ll;
	llplmn e;

	// search ll
	e.plmn=plmn;
	if( !(ll=sglib_llplmn_find_member(o->ll_root,&e)) ) {
		syslog(LOG_ERR,"ll not found - mcc=%d,mnc=%d",plmn->mcc,plmn->mnc);
		goto err;
	}

	// remove ll from the llplmn
	sglib_llplmn_delete(&o->ll_root,ll);
	// destroy ll
	free(ll);

	return 0;

err:
	return -1;
}

struct plmn_t* llplmn_add(struct llplmn_t* o,struct plmn_t* plmn,int copy)
{
	llplmn* ll;

	// create one
	if(!(ll=malloc(sizeof(*ll)))) {
		syslog(LOG_ERR,"ll memory allocation failure - %s",strerror(errno));
		goto err;
	}

	// init ll
	memset(ll,0,sizeof(*ll));
	if(copy) {
		ll->plmn=&(ll->builtin_plmn);
		memcpy(ll->plmn,plmn,sizeof(*plmn));
	}
	else {
		ll->plmn=plmn;
	}

	sglib_llplmn_add(&o->ll_root,ll);

	return ll->plmn;

err:
	return 0;
}

void llplmn_destroy(struct llplmn_t* o)
{
	struct sglib_llplmn_iterator it;
	llplmn* ll;


	for(ll=sglib_llplmn_it_init(&it,o->ll_root); ll!=NULL; ll=sglib_llplmn_it_next(&it)) {
		// destroy rbtt
		free(ll);
	}

	free(o);
}

struct llplmn_t* llplmn_create()
{
	struct llplmn_t* o=NULL;

	// allocate memory
	o=malloc(sizeof(*o));
	if(!o) {
		syslog(LOG_ERR,"ll object memory allocation failure - %s",strerror(errno));
		goto err;
	}

	// reset memory
	memset(o,0,sizeof(*o));

	return o;

err:
	if(o)
		free(o);

	return NULL;
}

void llplmn_sort(struct llplmn_t* o)
{
	sglib_llplmn_sort(&o->ll_root);
}

struct plmn_t* llplmn_get_next(struct llplmn_t* o)
{
	llplmn* ll;

	// init ll iterator
	if(!(ll=sglib_llplmn_it_next(&o->it_ll)))
		return NULL;

	return ll->plmn;
}

struct plmn_t* llplmn_get_first(struct llplmn_t* o)
{
	llplmn* ll;

	// init ll iterator
	if(!(ll=sglib_llplmn_it_init(&o->it_ll,o->ll_root)))
		return NULL;

	return ll->plmn;
}

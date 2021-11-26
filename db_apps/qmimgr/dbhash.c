#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include "dbhash.h"
#include "minilib.h"

int dbhash_lookup(struct dbhash_t* hash,const char* str)
{
	struct entry e;
	struct entry* ep;

	e.key=(char*)str;
	if(hsearch_r(e,FIND,&ep,&hash->htab)<0)
		return -1;

	if(ep==NULL)
		return -1;

	return (int)ep->data;
}

void dbhash_destroy(struct dbhash_t* hash)
{
	if(!hash)
		return;

	// destroy hash
	if(hash->data_created)
		hdestroy_r(&hash->htab);

	_free(hash);
}

static int dbhash_build(struct dbhash_t* hash, const struct dbhash_element_t* elements,int element_count)
{
	struct entry e;
	struct entry* ep;
	const struct dbhash_element_t* element;
	int i;


	for(i=0;i<element_count;i++) {
		element=&elements[i];

		e.key=(char*)element->str;
		e.data=(void*)(element->idx);

		if( hsearch_r(e,ENTER,&ep,&hash->htab)<0 ) {
			SYSLOG(LOG_ERROR,"failed from hsearch_r()");
			goto err;
		}

		if(ep==NULL) {
			SYSLOG(LOG_ERROR,"entry failed");
			goto err;
		}
	}

	return 0;

err:
	return -1;
}

struct dbhash_t* dbhash_create(const struct dbhash_element_t elements[],int element_count)
{
	struct dbhash_t* hash;

	// create the object
	hash=(struct dbhash_t*)_malloc(sizeof(struct dbhash_t));
	if(!hash) {
		SYSLOG(LOG_ERROR,"failed to allocate dbhash_t - size=%d",sizeof(struct dbhash_t));
		goto err;
	}

	// create hash
	if(hcreate_r(element_count,&hash->htab)<0) {
		SYSLOG(LOG_ERROR,"failed to create hash - size=%d",element_count);
		goto err;
	}

	hash->data_created=1;

	// build tree
	if( dbhash_build(hash,elements,element_count)<0 )
		goto err;

	return hash;

err:
	dbhash_destroy(hash);

	return NULL;
}

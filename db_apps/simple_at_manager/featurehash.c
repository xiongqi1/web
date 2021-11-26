//#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include "featurehash.h"


void featurehash_destroy(struct featurehash_t* hash)
{
	if(!hash)
		return;
	
	// destroy hash
	if(hash->data_created) {
#ifdef _GNU_SOURCE
		hdestroy_r(&hash->htab);
#else
		hdestroy();
#endif
	}
	
	free(hash);
}

static int featurehash_lookup(struct featurehash_t* hash,const char* str)
{
	struct entry e;
	struct entry* ep;
	
	e.key=(char*)str;

#ifdef _GNU_SOURCE
	if(hsearch_r(e,FIND,&ep,&hash->htab)<0)
#else
	if((ep=hsearch(e,FIND))==NULL)
		return -1;
#endif
	
	if(ep==NULL)
		return -1;
	
	return (int)ep->data;
}

int featurehash_is_enabled(struct featurehash_t* hash,const char* str)
{
	int enabled;
	
	enabled=featurehash_lookup(hash,str);
	if(enabled<0) 
		enabled=featurehash_lookup(hash,FEATUREHASH_ALL);
	
	return enabled;
}

int featurehash_add(struct featurehash_t* hash, const char* str,const int enable)
{
	struct entry e;
	struct entry* ep;
	
	e.key=(char*)str;
	e.data=(void*)(enable);
	
#ifdef _GNU_SOURCE
	if( hsearch_r(e,ENTER,&ep,&hash->htab)<0 ) {
#else
	if( (ep=hsearch(e,ENTER))==NULL) {
#endif
		syslog(LOG_ERR,"failed from hsearch_r() - %s",strerror(errno));
		goto err;
	}
	
	if(ep==NULL) {
		syslog(LOG_ERR,"entry failed - %s",strerror(errno));
		goto err;	
	}
		
	return 0;	
	
err:
	return -1;	
}

struct featurehash_t* featurehash_create(void)
{
	struct featurehash_t* hash;
	
	// create the object
	hash=(struct featurehash_t*)malloc(sizeof(struct featurehash_t));
	if(!hash) {
		syslog(LOG_ERR,"failed to allocate featurehash_t - size=%d",sizeof(struct featurehash_t));
		goto err;
	}
	
	// create hash
#ifdef _GNU_SOURCE
	if(hcreate_r(FEATUREHASH_MAX,&hash->htab)<0) {
#else
	if(hcreate(FEATUREHASH_MAX)<0) {
#endif
		syslog(LOG_ERR,"failed to create hash - size=%d",FEATUREHASH_MAX);
		goto err;
	}
	
	hash->data_created=1;
	
	return hash;
	
err:
	featurehash_destroy(hash);
	
	return NULL;	
}

static struct featurehash_t* _feature=0;

int is_enabled_feature(const char* str)
{
	return featurehash_is_enabled(_feature,str);
}

int add_feature(const char* str,const int enable)
{
	return featurehash_add(_feature,str,enable);
}

int init_feature()
{
	_feature=featurehash_create();
	if(_feature==0)
		return -1;
	
	return 0;
}

void fini_feature()
{
	featurehash_destroy(_feature);
}

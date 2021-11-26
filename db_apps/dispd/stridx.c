/*!
 * Copyright Notice:
 * Copyright (C) 2012 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include <sglib.h>

#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// define dllink
typedef struct dllist {
	char* str;
	
	long ref;
	
	struct dllist* next;
	struct dllist* prev;
	
} dllist;

#define DLLIST_COMPARATOR(e1, e2) ( strcmp((e1)->str,(e2)->str) )
SGLIB_DEFINE_DL_LIST_PROTOTYPES(dllist, DLLIST_COMPARATOR, next, prev);
SGLIB_DEFINE_DL_LIST_FUNCTIONS(dllist, DLLIST_COMPARATOR, next, prev);


// define rbtree
typedef struct rbtree {
	long hash_key;
	
	char color_field;
	
	struct rbtree* left;
	struct rbtree* right;
		
	dllist* dl_root;
} rbtree;

#define CMPARATOR(x,y) ((x)->hash_key-(y)->hash_key)
SGLIB_DEFINE_RBTREE_PROTOTYPES(rbtree, left, right, color_field, CMPARATOR);
SGLIB_DEFINE_RBTREE_FUNCTIONS(rbtree, left, right, color_field, CMPARATOR);

// define strbt (main object)
struct stridx_t {
	int alloc;
	
	rbtree* rbt_root;
	
	struct sglib_rbtree_iterator it_rbt;
	struct sglib_dllist_iterator it_dl;
};

static long stridx_get_hash_key(const char* str)
{
	long hash = 5381;
	int c;

	// use DJB2a hash - not the best but simple and quick
	while ( (c = *str++)!=0 )
		hash = ((hash << 5) + hash) ^ c; // hash * 33 ^ c

	return hash & ~0x80000000;
}


static rbtree* _find_rbt(struct stridx_t* si, long hash_key)
{
	rbtree e;
	
	e.hash_key=hash_key;
	return sglib_rbtree_find_member(si->rbt_root,&e);
}

static dllist* _find_dl_in_rbt(struct stridx_t* si, rbtree* rbt, const char* str)
{
	dllist* dl;
	struct sglib_dllist_iterator it;

	for(dl=sglib_dllist_it_init(&it,rbt->dl_root); dl!=NULL; dl=sglib_dllist_it_next(&it)) {
		if(!strcmp(dl->str,str))
			return dl;
	}
	
	return NULL;
}

static dllist* _find_dl(struct stridx_t* si,long hash_key,const char* str)
{
	rbtree* rbt;
	
	// search rbt
	if( !(rbt=_find_rbt(si,hash_key)) )
		return NULL;
	
	// search dl
	return _find_dl_in_rbt(si,rbt,str);
}

long stridx_find(struct stridx_t* si,const char* str)
{
	long hash_key;
	dllist* dl;
	
	// build 
	hash_key=stridx_get_hash_key(str);
	
	// find
	if( !(dl=_find_dl(si,hash_key,str)) )
		return -1;
	
	return dl->ref;
}

static void _destroy_rbt(rbtree* rbt)
{
	free(rbt);
}

static rbtree* _create_rbt()
{
	rbtree* rbt;
	
	if( !(rbt=malloc(sizeof(*rbt))) )
		return NULL;
		
	memset(rbt,0,sizeof(*rbt));
	return rbt;
}

static void _destroy_dl(dllist* dl)
{
	if(dl->str)
		free(dl->str);
	
	free(dl);
}

static dllist* _create_dl(const char* str)
{
	dllist* dl=NULL;
	
	// allocate dl
	if( !(dl=malloc(sizeof(*dl))) )
		goto err;
	
	// reset dl
	memset(dl,0,sizeof(*dl));
	
	if(str)
		dl->str=strdup(str);
	
	return dl;
	
err:
	if(dl)
		_destroy_dl(dl);
			
	return NULL;
}			

int stridx_del(struct stridx_t* si,const char* str)
{
	rbtree* rbt;
	dllist* dl;
	
	long hash_key;
	
	// get key
	hash_key=stridx_get_hash_key(str);
	
	// search rbt
	if( !(rbt=_find_rbt(si,hash_key)) ) {
		syslog(LOG_ERR,"rbt not found - key=%ld",hash_key);
		goto err;
	}
	
	// search dl
	if( !(dl=_find_dl_in_rbt(si,rbt,str)) ) {
		syslog(LOG_ERR,"dl not found - key=%ld,str=%s",hash_key,str);
		goto err;
	}
	
	// remove dl from dllist
	sglib_dllist_delete(&rbt->dl_root, dl);
	// free dl
	_destroy_dl(dl);
	
	// delete rbt if no dl exists
	if(!rbt->dl_root) {
		// remove rbt from the rbtree
		sglib_rbtree_delete(&si->rbt_root,rbt);
		// destroy rbt
		_destroy_rbt(rbt);
	}
	
	return 0;
err:
	return -1;
}	

int stridx_add(struct stridx_t* si,const char* str,long ref)
{
	rbtree* rbt;
	dllist* dl;
	
	long hash_key;
	
	// get key
	hash_key=stridx_get_hash_key(str);
	
	// find rbt or create one
	if( !(rbt=_find_rbt(si,hash_key)) ) {
		if( !(rbt=_create_rbt()) ) {
			syslog(LOG_ERR,"rbt memory allocation failure - %s",strerror(errno));
			goto err;
		}
		
		// add rbt
		rbt->hash_key=hash_key;
		sglib_rbtree_add(&si->rbt_root,rbt);
	}
	else {
		// search dl - bypass if already exists
		if( (dl=_find_dl_in_rbt(si,rbt,str))!=0 ) {
			syslog(LOG_ERR,"str already exists - key=%ld,str=%s",hash_key,str);
			goto err;
		}
	}
	
	// create dl
	if( !(dl=_create_dl(str)) ) {
		syslog(LOG_ERR,"dl memory allocation failure - %s",strerror(errno));
		goto err;
	}
	
	// copy content
	dl->ref=ref;
	       
	// add
	sglib_dllist_add(&rbt->dl_root, dl);
	
	return 0; 
			
err:
	return -1;
}

void stridx_destroy(struct stridx_t* si)
{
	struct sglib_rbtree_iterator it;
	struct sglib_dllist_iterator it_dl;
	rbtree* rbt;
	dllist* dl;
	
	
	for(rbt=sglib_rbtree_it_init(&it,si->rbt_root); rbt!=NULL; rbt=sglib_rbtree_it_next(&it)) {
		// destroy dl in the rbt
		for(dl=sglib_dllist_it_init(&it_dl,rbt->dl_root); dl!=NULL; dl=sglib_dllist_it_next(&it_dl)) {
			_destroy_dl(dl);
		}
		
		// destroy rbtt
		_destroy_rbt(rbt);
	}
	
	if(si->alloc)
		free(si);
}

void stridx_init(struct stridx_t* si)
{
	// reset memory
	memset(si,0,sizeof(*si));
}

struct stridx_t* stridx_create(void)
{
	struct stridx_t* si=NULL;
	
	// allocate memory
	si=malloc(sizeof(*si));
	if(!si) {
		syslog(LOG_ERR,"si memory allocation failure - %s",strerror(errno));
		goto err;
	}
	
	stridx_init(si);
	
	si->alloc=1;
	
	return si;
	
err:
	if(si)
		free(si);
			
	return NULL;
}

static long stridx_get_first_or_next(struct stridx_t* si,const char** str,int init)
{
	rbtree* rbt;
	dllist* dl;
	
	if(init) {
		// init rbt iterator
		if(!(rbt=sglib_rbtree_it_init(&si->it_rbt,si->rbt_root)))
			return -1;
		// init dl iterator
		dl=sglib_dllist_it_init(&si->it_dl,rbt->dl_root);
	}
	else {
		// increase dl iterator
		if(!(dl=sglib_dllist_it_next(&si->it_dl))) {
			// increase rbt iterator
			if( (rbt=sglib_rbtree_it_next(&si->it_rbt))!=0 ) 
				dl=sglib_dllist_it_init(&si->it_dl,rbt->dl_root);
			else
				dl=NULL;
		}
	}
	
	if(!dl)
		return -1;
		
	if(str)
		*str=dl->str;
	
	return dl->ref;
}

long stridx_get_next(struct stridx_t* si,const char** str)
{
	return stridx_get_first_or_next(si,str,0);
}

long stridx_get_first(struct stridx_t* si,const char** str)
{
	return stridx_get_first_or_next(si,str,1);
}


#ifdef MODULE_TEST

void rbtreehash_dlist_test() 
{
	struct stridx_t* si;
	
	si=stridx_create();
	
	if(!si) {
		fprintf(stderr,"stridx_create() failed\n");
		exit(-1);
	}

	int i;
	char str[256];
	char* str2;
	
	int ref;
	
	for(i=0;i<256;i++) {
		
		// set str
		snprintf(str,sizeof(str),"str-%d",i);
		// add	
		printf("adding %s\n",str);
		if( stridx_add(si,str,i)<0 ) {
			fprintf(stderr,"stridx_add() failed - str=%s,idx=%d\n",str,i);
			exit(-1);
		}
	}
	
	// test - search1
	str2="str-1";
	if( (ref=stridx_find(si,str2)<0 ) ) {
		fprintf(stderr,"cannot found %s\n",str2);
		exit(-1);
	}
	printf("str=%s, idx=%d\n",str,ref);
	
	// delete everything
	for(i=0;i<256;i++) {
		// set str
		snprintf(str,sizeof(str),"str-%d",i);
		printf("deleting %s\n",str);
		if( stridx_del(si,str)<0 ) {
			fprintf(stderr,"stridx_add() failed - str=%s,idx=%d\n",str,i);
		}
	}
	
	// add	
	ref=100;
	str2="str-100";
	if( stridx_add(si,str2,ref)<0 ) {
		fprintf(stderr,"stridx_add() failed - str=%s,idx=%d\n",str,ref);
		exit(-1);
	}
	
	stridx_destroy(si);
}

int main(int argc,char* argv[])
{
	int i;
	
	for(i=0;i<1000;i++) {
		printf("test #%d\n",i);
		rbtreehash_dlist_test();
	}
	
	return 0;
}

#endif

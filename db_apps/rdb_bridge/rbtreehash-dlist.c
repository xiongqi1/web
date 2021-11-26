/*
 * RBT(Red-Black Tree) for RDB bridge daemon
 *
 * Original codes copied from db_apps/timedaemon
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "debug.h"

#include <sglib.h>

#include <cdcs_syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// define dllink
typedef struct dllist {
	char* str;

	void* ref;
	int ref_alloc;

	struct dllist* next;
	struct dllist* prev;

} dllist;

#define DLLIST_COMPARATOR(e1, e2) ( strcmp((e1)->str,(e2)->str) )
SGLIB_DEFINE_DL_LIST_PROTOTYPES(dllist, DLLIST_COMPARATOR, next, prev);
SGLIB_DEFINE_DL_LIST_FUNCTIONS(dllist, DLLIST_COMPARATOR, next, prev);


// define rbtree
typedef struct rbtree {
	int hash_key;

	char color_field;

	struct rbtree* left;
	struct rbtree* right;

	dllist* dl_root;
} rbtree;

#define CMPARATOR(x,y) ( ((x)->hash_key)-((y)->hash_key) )
SGLIB_DEFINE_RBTREE_PROTOTYPES(rbtree, left, right, color_field, CMPARATOR);
SGLIB_DEFINE_RBTREE_FUNCTIONS(rbtree, left, right, color_field, CMPARATOR);

// define strbt (main object)
struct strbt_t {
	int ref_len;

	rbtree* rbt_root;

	struct sglib_rbtree_iterator it_rbt;
	struct sglib_dllist_iterator it_dl;
};

static int strbt_get_hash_key(const char* str)
{
	int hash = 5381;
	int c;

	// use DJB2a hash - not the best but simple and quick
	while ( (c = *str++)!=0 )
		hash = ((hash << 5) + hash) ^ c; // hash * 33 ^ c

	return hash & ~0x80000000;
}


static rbtree* _find_rbt(struct strbt_t* srl, int hash_key)
{
	rbtree e;

	e.hash_key=hash_key;
	return sglib_rbtree_find_member(srl->rbt_root,&e);
}

static dllist* _find_dl_in_rbt(struct strbt_t* srl, rbtree* rbt, const char* str)
{
	dllist* dl;
	struct sglib_dllist_iterator it;

	for(dl=sglib_dllist_it_init(&it,rbt->dl_root); dl!=NULL; dl=sglib_dllist_it_next(&it)) {
		if(!strcmp(dl->str,str))
			return dl;
	}

	return NULL;
}

static dllist* _find_dl(struct strbt_t* srl,int hash_key,const char* str)
{
	rbtree* rbt;

	// search rbt
	if( !(rbt=_find_rbt(srl,hash_key)) )
		return NULL;

	// search dl
	return _find_dl_in_rbt(srl,rbt,str);
}

void* strbt_find(struct strbt_t* srl,const char* str)
{
	int hash_key;
	dllist* dl;

	// build
	hash_key=strbt_get_hash_key(str);

	// find
	if( !(dl=_find_dl(srl,hash_key,str)) )
		return NULL;

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
	if(dl->ref_alloc && dl->ref)
		free(dl->ref);

	if(dl->str)
		free(dl->str);

	free(dl);
}

static dllist* _create_dl(int ref_len,const char* str)
{
	dllist* dl=NULL;

	// allocate dl
	if( !(dl=malloc(sizeof(*dl))) )
		goto err;
	// reset dl
	memset(dl,0,sizeof(*dl));

	// allocate content if required
	if(ref_len) {
		if( !(dl->ref=malloc(ref_len)) )
			goto err;

		dl->ref_alloc=1;
	}

	if(str)
		dl->str=strdup(str);

	return dl;

err:
	if(dl)
		_destroy_dl(dl);

	return NULL;
}

int strbt_del(struct strbt_t* srl,const char* str)
{
	rbtree* rbt;
	dllist* dl;

	int hash_key;

	// get key
	hash_key=strbt_get_hash_key(str);

	// search rbt
	if( !(rbt=_find_rbt(srl,hash_key)) ) {
		ERR("rbt not found - key=%d",hash_key);
		goto err;
	}

	// search dl
	if( !(dl=_find_dl_in_rbt(srl,rbt,str)) ) {
		ERR("dl not found - key=%d,str=%s",hash_key,str);
		goto err;
	}

	// remove dl from dllist
	sglib_dllist_delete(&rbt->dl_root, dl);
	// free dl
	_destroy_dl(dl);

	// delete rbt if no dl exists
	if(!rbt->dl_root) {
		// remove rbt from the rbtree
		sglib_rbtree_delete(&srl->rbt_root,rbt);
		// destroy rbt
		_destroy_rbt(rbt);
	}

	return 0;
err:
	return -1;
}

int strbt_add(struct strbt_t* srl,const char* str,void* ref)
{
	rbtree* rbt;
	dllist* dl;

	int hash_key;

	// get key
	hash_key=strbt_get_hash_key(str);

	// find rbt or create one
	if( !(rbt=_find_rbt(srl,hash_key)) ) {
		if( !(rbt=_create_rbt()) ) {
			ERR("rbt memory allocation failure - %s",strerror(errno));
			goto err;
		}

		// add rbt
		rbt->hash_key=hash_key;
		sglib_rbtree_add(&srl->rbt_root,rbt);
	}
	else {
		// search dl - bypass if already exists
		if( (dl=_find_dl_in_rbt(srl,rbt,str))!=0 ) {
			ERR("str already exists - key=%ld,str=%s",hash_key,str);
			goto err;
		}
	}

	// create dl
	if( !(dl=_create_dl(srl->ref_len,str)) ) {
		ERR("dl memory allocation failure - %s",strerror(errno));
		goto err;
	}

	// copy content
	if(srl->ref_len)
		memcpy(dl->ref,ref,srl->ref_len);
	else
		dl->ref=ref;

	// add
	sglib_dllist_add(&rbt->dl_root, dl);

	return 0;

err:
	return -1;
}

void strbt_destroy(struct strbt_t* srl)
{
	struct sglib_rbtree_iterator it;
	struct sglib_dllist_iterator it_dl;
	rbtree* rbt;
	dllist* dl;


	for(rbt=sglib_rbtree_it_init(&it,srl->rbt_root); rbt!=NULL; rbt=sglib_rbtree_it_next(&it)) {
		// destroy dl in the rbt
		for(dl=sglib_dllist_it_init(&it_dl,rbt->dl_root); dl!=NULL; dl=sglib_dllist_it_next(&it_dl)) {
			_destroy_dl(dl);
		}

		// destroy rbtt
		_destroy_rbt(rbt);
	}

	free(srl);
}

struct strbt_t* strbt_create(int ref_len)
{
	struct strbt_t* srl;

	// allocate memory
	srl=malloc(sizeof(*srl));
	if(!srl) {
		NTCLOG_ERR("srl memory allocation failure - %s",strerror(errno));
		return NULL;
	}

	// reset memory
	memset(srl,0,sizeof(*srl));

	srl->ref_len=ref_len;

	return srl;
}

static void* strbt_get_first_or_next(struct strbt_t* srl,const char** str,int init)
{
	rbtree* rbt;
	dllist* dl;

	if(init) {
		// init rbt iterator
		if(!(rbt=sglib_rbtree_it_init(&srl->it_rbt,srl->rbt_root)))
			return NULL;
		// init dl iterator
		dl=sglib_dllist_it_init(&srl->it_dl,rbt->dl_root);
	}
	else {
		// increase dl iterator
		if(!(dl=sglib_dllist_it_next(&srl->it_dl))) {
			// increase rbt iterator
			if( (rbt=sglib_rbtree_it_next(&srl->it_rbt))!=0 )
				dl=sglib_dllist_it_init(&srl->it_dl,rbt->dl_root);
			else
				dl=NULL;
		}
	}

	if(!dl)
		return NULL;

	if(str)
		*str=dl->str;

	return dl->ref;
}

void* strbt_get_next(struct strbt_t* srl,const char** str)
{
	return strbt_get_first_or_next(srl,str,0);
}

void* strbt_get_first(struct strbt_t* srl,const char** str)
{
	return strbt_get_first_or_next(srl,str,1);
}


#ifdef MODULE_TEST

struct rbtreehash_dlist_test_dummy_t {
	int idx;
};

void rbtreehash_dlist_test()
{
	struct strbt_t* srl;
	struct rbtreehash_dlist_test_dummy_t dummy;
	struct rbtreehash_dlist_test_dummy_t* p;

	srl=strbt_create(sizeof(struct rbtreehash_dlist_test_dummy_t));

	if(!srl) {
		fprintf(stderr,"strbt_create() failed\n");
		exit(-1);
	}

	int i;
	char str[256];
	char* str2;

	for(i=0;i<256;i++) {

		// set str
		snprintf(str,sizeof(str),"str-%d",i);
		// set ref
		memset(&dummy,0,sizeof(dummy));
		dummy.idx=i;
		// add
		printf("adding %s\n",str);
		if( strbt_add(srl,str,&dummy)<0 ) {
			fprintf(stderr,"strbt_add() failed - str=%s,idx=%d\n",str,dummy.idx);
			exit(-1);
		}
	}

	// test - search1
	str2="str-1";
	if( !(p=strbt_find(srl,str2) ) ) {
		fprintf(stderr,"cannot found %s\n",str2);
		exit(-1);
	}
	printf("str=%s, idx=%d\n",str,p->idx);

	// test - search2
	str2="str-100";
	if( !(p=strbt_find(srl,str2) ) ) {
		fprintf(stderr,"cannot found %s\n",str2);
		exit(-1);
	}
	printf("str=%s, idx=%d\n",str,p->idx);

	// test - search3
	str2="str-255";
	if( !(p=strbt_find(srl,str2) ) ) {
		fprintf(stderr,"cannot found %s\n",str2);
		exit(-1);
	}
	printf("str=%s, idx=%d\n",str,p->idx);

	// delete
	str2="str-100";
	if( strbt_del(srl,str2)<0 ) {
		fprintf(stderr,"failed to delete %s\n",str2);
		exit(-1);
	}


	// test - search2
	str2="str-100";
	if( !(p=strbt_find(srl,str2) ) ) {
		fprintf(stderr,"cannot found %s - deleted\n",str2);
	}

	// delete everything
	for(i=0;i<256;i++) {

		// set str
		snprintf(str,sizeof(str),"str-%d",i);
		printf("deleting %s\n",str);
		if( strbt_del(srl,str)<0 ) {
			fprintf(stderr,"strbt_add() failed - str=%s,idx=%d\n",str,dummy.idx);
		}
	}

	// add
	str2="str-100";
	if( strbt_add(srl,str2,&dummy)<0 ) {
		fprintf(stderr,"strbt_add() failed - str=%s,idx=%d\n",str,dummy.idx);
		exit(-1);
	}

	strbt_destroy(srl);
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

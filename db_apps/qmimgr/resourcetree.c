#include "resourcetree.h"

#include "minilib.h"

void resourcetree_destroy(struct resourcetree_t* res)
{
	if(!res)
		return;

	generictree_destroy(res->tree);

	_free(res);
}

const char* resourcetree_lookup(struct resourcetree_t* res,unsigned long long idx)
{
	return generictree_find(res->tree,idx);
}

static int resourcetree_build(struct resourcetree_t* res,const struct resourcetree_element_t elements[],int element_count)
{
	int i;
	const struct resourcetree_element_t* element;
	const char* prev;

	for(i=0;i<element_count;i++) {
		element=&elements[i];

		if(generictree_add(res->tree,element->idx,element->str)<0) {
			prev=generictree_find(res->tree,element->idx);
			SYSLOG(LOG_ERROR,"failed to add - idx=%lld(0x%016llx),str=%s,prev=%s",element->idx,element->idx,element->str,prev);
			goto err;
		}
	}

	return 0;

err:
	return -1;

}

struct resourcetree_t* resourcetree_create(const struct resourcetree_element_t elements[],int element_count)
{
	struct resourcetree_t* res;

	// create the object
	res=(struct resourcetree_t*)_malloc(sizeof(struct resourcetree_t));
	if(!res) {
		SYSLOG(LOG_ERROR,"failed to allocate resourcetree_t - size=%d",sizeof(struct resourcetree_t));
		goto err;
	}

	// create tree
	res->tree=generictree_create();
	if(res->tree<0)
		goto err;

	if( resourcetree_build(res,elements,element_count)<0 )
		goto err;

	return res;
err:

	return NULL;
}

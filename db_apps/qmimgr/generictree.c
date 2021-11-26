#define _GNU_SOURCE
#include <search.h>
#include <stdlib.h>

#include "generictree.h"

#include "minilib.h"

struct generictree_element_t {
	unsigned long long key;
	const void* content;
};

static int generictree_compare(const void *element1, const void *element2)
{
	struct generictree_element_t* tree_element1;
	struct generictree_element_t* tree_element2;

	tree_element1=(struct generictree_element_t*)element1;
	tree_element2=(struct generictree_element_t*)element2;

	if(tree_element1->key==tree_element2->key)
		return 0;
	else if (tree_element1->key<tree_element2->key)
		return -1;

	return 1;
}

const void* generictree_find(struct generictree_t* tree,unsigned long long  key)
{
	struct generictree_element_t element;
	struct generictree_element_t** element_found;

	element.key=key;
	element.content=0;

	element_found=tfind(&element,&tree->root,generictree_compare);
	if(!element_found) {
		SYSLOG(LOG_OPERATION,"no element found - key=%lld(0x%016llx)",key,key);
		goto err;
	}

	return (*element_found)->content;

err:
	return NULL;
}

int generictree_del(struct generictree_t* tree,unsigned long long key)
{
	struct generictree_element_t element;
	struct generictree_element_t** element_found;
	struct generictree_element_t* element_del;

	element.key=key;
	element.content=0;

	element_found=tfind(&element,&tree->root,generictree_compare);
	if(!element_found) {
		SYSLOG(LOG_OPERATION,"no element found - key=%lld",key);
		goto err;
	}

	// keep the element
	element_del=*element_found;

	if(!tdelete(*element_found,&tree->root,generictree_compare)) {
		SYSLOG(LOG_ERROR,"failed to delete an element - key=%lld",key);
		goto err;
	}

	free(element_del);

	return 0;
err:
	return -1;
}

int generictree_add(struct generictree_t* tree,unsigned long long key,const void* content)
{
	struct generictree_element_t* element;
	struct generictree_element_t** element_added;

	// create a new element
	element=(struct generictree_element_t*)_malloc(sizeof(struct generictree_element_t));
	if(!element) {
		SYSLOG(LOG_ERROR,"failed to allocate generictree_element_t");
		goto err;
	}

	element->key=key;
	element->content=content;

	// search and add
	element_added=(struct generictree_element_t**)tsearch(element,&tree->root,generictree_compare);
	if(!element_added) {
		SYSLOG(LOG_ERROR,"failed to add a new element to the tree");
		goto err;
	}

	// error if exists
	if(*element_added != element) {
		SYSLOG(LOG_ERROR,"element already exists - %lld(0x%016llx)",key,key);
		goto err;
	}

	return 0;

err:
	_free(element);
	return -1;
}

void generictree_set_destroy_content_callback(struct generictree_t* tree,destroy_content_callback destroy_content)
{
	tree->destroy_content=destroy_content;
}

void generictree_destroy(struct generictree_t* tree)
{
	if(!tree)
		return;

	// destroy tree
	if(tree->destroy_content)
		tdestroy(tree->root,tree->destroy_content);
	else
		tdestroy(tree->root,free);

	_free(tree);
}

struct generictree_t* generictree_create(void)
{
	struct generictree_t* tree;

	// create the object
	tree=(struct generictree_t*)_malloc(sizeof(struct generictree_t));
	if(!tree) {
		SYSLOG(LOG_ERROR,"failed to allocate generictree_t - size=%d",sizeof(struct generictree_t));
		goto err;
	}

	return tree;

err:
	generictree_destroy(tree);

	return NULL;

}

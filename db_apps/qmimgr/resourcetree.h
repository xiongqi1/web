#ifndef __RESOURCETREE_H__
#define __RESOURCETREE_H__

#include "generictree.h"

struct resourcetree_t {
	struct generictree_t* tree;
};

struct resourcetree_element_t {
	unsigned long long idx;
	const char* str;
};


void resourcetree_destroy(struct resourcetree_t* res);
const char* resourcetree_lookup(struct resourcetree_t* res,unsigned long long idx);
struct resourcetree_t* resourcetree_create(const struct resourcetree_element_t elements[],int element_count);


#endif

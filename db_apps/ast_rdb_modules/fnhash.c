#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include "fnhash.h"

void* fnhash_lookup(struct fnhash_t* hash, const char* str)
{
	struct entry e;
	struct entry* ep;

	e.key = (char*)str;
	if(hsearch_r(e, FIND, &ep, &hash->htab) < 0)
		return NULL;

	if(ep == NULL)
		return NULL;

	return ep->data;
}

void fnhash_destroy(struct fnhash_t* hash)
{
	if(!hash)
		return;

	// destroy hash
	if(hash->data_created)
		hdestroy_r(&hash->htab);

	free(hash);
}

static int fnhash_build(struct fnhash_t* hash, const struct fnhash_element_t* elements, int element_count)
{
	struct entry e;
	struct entry* ep;
	const struct fnhash_element_t* element;
	int i;


	for(i = 0; i < element_count; i++) {
		element = &elements[i];

		e.key = (char*)element->str;
		e.data = (void*)(element->func);

		if(hsearch_r(e, ENTER, &ep, &hash->htab) < 0) {
			syslog(LOG_ERR, "failed from hsearch_r()");
			goto err;
		}

		if(ep == NULL) {
			syslog(LOG_ERR, "entry failed");
			goto err;
		}
	}

	return 0;

err:
	return -1;
}

struct fnhash_t* fnhash_create(const struct fnhash_element_t elements[], int element_count)
{
	struct fnhash_t* hash;

	// create the object
	hash = (struct fnhash_t*)calloc(1, sizeof(*hash));
	if(!hash) {
		syslog(LOG_ERR, "failed to allocate fnhash_t - size=%d", sizeof(struct fnhash_t));
		goto err;
	}

	// create hash
	if(hcreate_r(element_count, &hash->htab) < 0) {
		syslog(LOG_ERR, "failed to create hash - size=%d", element_count);
		goto err;
	}

	hash->data_created = 1;

	// build tree
	if(fnhash_build(hash, elements, element_count) < 0)
		goto err;

	return hash;

err:
	fnhash_destroy(hash);

	return NULL;
}

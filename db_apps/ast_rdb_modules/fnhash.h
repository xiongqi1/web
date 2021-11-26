#ifndef __FNHASH_H__
#define __FNHASH_H__

#include <search.h>

struct fnhash_t {
	int data_created;
	struct hsearch_data htab;
};

struct fnhash_element_t {
	const char* str;
	void* func;
};

void* fnhash_lookup(struct fnhash_t* hash, const char* str);
void fnhash_destroy(struct fnhash_t* hash);
struct fnhash_t* fnhash_create(const struct fnhash_element_t elements[], int element_count);

#endif

#ifndef __DBHASH_H__
#define __DBHASH_H__

#define _GNU_SOURCE

#include <search.h>

struct dbhash_t {
	int data_created;
	struct hsearch_data htab;
};

struct dbhash_element_t {
	const char* str;
	int idx;
};

int dbhash_lookup(struct dbhash_t* hash, const char* str);
void dbhash_destroy(struct dbhash_t* hash);
struct dbhash_t* dbhash_create(const struct dbhash_element_t elements[], int element_count);

#endif

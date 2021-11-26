#ifndef __STRQUEUE_H__
#define __STRQUEUE_H__

#include "linkedlist.h"

#define STRQUEUE_STR_BLOCK	64	// growing block
#define STRQUEUE_MAX_STR_LEN	1024	// max line byte count
#define STRQUEUE_MAX_STR	1024	// total line count

struct strqueue_element_t {
	struct linkedlist list;

	char* str;
	int alloc;

	unsigned short tran_id;
	int timeout;

	int* count;
};

struct strqueue_t {
	struct strqueue_element_t active;
	struct strqueue_element_t* active_tail;
	int active_count;

	struct strqueue_element_t inactive;

	struct strqueue_element_t* walk;

	int total_elements;
};

struct strqueue_t* strqueue_create();
void strqueue_destroy(struct strqueue_t* q);

const char* strqueue_add(struct strqueue_t* q, const struct strqueue_element_t* new_el);
int strqueue_remove(struct strqueue_t* q,int count);

struct strqueue_element_t* strqueue_walk_to(struct strqueue_t* q,int count);
struct strqueue_element_t* strqueue_walk_first(struct strqueue_t* q);
struct strqueue_element_t* strqueue_walk_next(struct strqueue_t* q);

int strqueue_is_empty(struct strqueue_t* q);

int strqueue_vomit(struct strqueue_t* q, struct strqueue_t* dst);
int strqueue_eat(struct strqueue_t* q, struct strqueue_t* dst);
void strqueue_debug_dump(struct strqueue_t* q);

#endif

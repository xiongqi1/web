#ifndef __GENERICTREE_H__
#define __GENERICTREE_H__


struct generictree_t;

typedef void (*destroy_content_callback)(void* content);

struct generictree_t {
	void* root;
	destroy_content_callback destroy_content;
};

void generictree_set_destroy_content_callback(struct generictree_t* tree,destroy_content_callback destroy_content);
const void* generictree_find(struct generictree_t* tree,unsigned long long key);
int generictree_del(struct generictree_t* tree,unsigned long long key);
int generictree_add(struct generictree_t* tree,unsigned long long key,const void* content);
void generictree_destroy(struct generictree_t* tree);
struct generictree_t* generictree_create(void);

#endif

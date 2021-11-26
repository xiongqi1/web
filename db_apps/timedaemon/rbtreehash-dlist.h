#ifndef __RBTREEHASH_DLIST_H__
#define __RBTREEHASH_DLIST_H__

struct strbt_t;

void* strbt_find(struct strbt_t* srl,const char* str);
int strbt_del(struct strbt_t* srl,const char* str);
int strbt_add(struct strbt_t* srl,const char* str,void* cont);
void strbt_destroy(struct strbt_t* srl);
struct strbt_t* strbt_create(int cont_len);


void* strbt_get_first(struct strbt_t* srl,const char** str);
void* strbt_get_next(struct strbt_t* srl,const char** str);



#endif

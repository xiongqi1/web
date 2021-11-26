#ifndef __RBTREEHASH_DLIST_H__
#define __RBTREEHASH_DLIST_H__

struct stridx_t;

long stridx_find(struct stridx_t* si,const char* str);
int stridx_del(struct stridx_t* si,const char* str);
int stridx_add(struct stridx_t* si,const char* str,long ref);
void stridx_destroy(struct stridx_t* si);
struct stridx_t* stridx_create(void);
void stridx_init(struct stridx_t* si);

long stridx_get_first(struct stridx_t* si,const char** str);
long stridx_get_next(struct stridx_t* si,const char** str);



#endif

#ifndef __STR2REF_H__
#define __STR2REF_H__

struct str2ref_t;

int str2ref_del(struct str2ref_t* si,const char* str);
int str2ref_add(struct str2ref_t* si,const char* str,void* ref);
void str2ref_destroy(struct str2ref_t* si);
struct str2ref_t* str2ref_create(void);
void str2ref_init(struct str2ref_t* si);

int str2ref_get_first(struct str2ref_t* si,const char** str,void** ref);
int str2ref_get_next(struct str2ref_t* si,const char** str,void** ref);

int str2ref_find_first(struct str2ref_t* si,const char* str,void** ref);
int str2ref_find_next(struct str2ref_t* si,const char* str,void** ref);

int str2ref_del_by_ref(struct str2ref_t* si,void* ref);

#endif

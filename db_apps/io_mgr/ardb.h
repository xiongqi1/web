#ifndef __ARDB_H__
#define __ARDB_H__

int ardb_init();
void ardb_fini();
int ardb_get_first_triggered();
int ardb_get_next_triggered();
int ardb_add_reference(const char* rdb_var,int ref);
int ardb_get_reference(const char* rdb_var);
int ardb_subscribe(const char* rdb_var,int ref, int persist);

#endif
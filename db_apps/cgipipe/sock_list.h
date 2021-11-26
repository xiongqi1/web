#ifndef __SOCK_LIST_H__
#define __SOCK_LIST_H__


struct sock_list_t {
	int sock_cnt;
	
	int idx;
	
	int* sock;
	int* ref;
};


#endif

#include "sock_list.h"

#include <stdlib.h>
#include <string.h>

int get_next_sock_idx_from_list(struct sock_list_t* l)
{
	for(;l->idx<l->sock_cnt;l->idx++) {
		if(l->sock[l->idx]>=0)
			return l->idx++;
	}
	
	return -1;
}

int get_first_sock_idx_from_list(struct sock_list_t* l)
{
	l->idx=0;
	return get_next_sock_idx_from_list(l);
}

void fini_sock_list(struct sock_list_t* l)
{
	if(l->sock) {
		free(l->sock);
		l->sock=0;
	}
	
	if(l->ref) {
		free(l->ref);
		l->ref=0;
	}
		
}	

int init_sock_list(struct sock_list_t* l, int cnt)
{
	memset(l,0,sizeof(*l));
	
	// allocate sock
	if( (l->sock=(int*)malloc(cnt*sizeof(int)))==0 )
		goto err;
	memset(l->sock,0xff,cnt*sizeof(int));
	
	// allocate ref
	if( (l->ref=(int*)malloc(cnt*sizeof(int)))==0 )
		goto err;
	memset(l->sock,0,cnt*sizeof(int));
	
	return 0;
	
err:
	fini_sock_list(l);
	
	return -1;
}

int remove_sock_from_list(struct sock_list_t* l, int sock)
{
	int i;
	
	for(i=0;i<l->sock_cnt;i++) {
		if(l->sock[i]==sock) {
			l->sock[i]=-1;
			return i;
		}
	}
	
	return -1;
}

int add_sock_to_list(struct sock_list_t* l, int sock)
{
	int i;
	
	for(i=0;i<l->sock_cnt;i++) {
		if(l->sock[i]<0) {
			l->sock[i]=sock;
			return i;
		}
	}
	
	return -1;
}


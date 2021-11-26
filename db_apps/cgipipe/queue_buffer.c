#include "queue_buffer.h"

#include <stdlib.h>
#include <string.h>

#define MAX(a,b) ( ((a)>(b))?(a):(b) )
#define MIN(a,b) ( ((a)>(b))?(b):(a) )

void fini_queue(struct queue_t* q)
{
	if(q->buf)
		free(q->buf);
	
	q->buf=0;
}

int get_queue_content_len(struct queue_t* q)
{
	return (q->t+q->buf_len-q->h)%q->buf_len;
}

int get_queue_free(struct queue_t* q)
{
	return (q->h+q->buf_len-q->t-1)%q->buf_len;
}

int remove_from_queue(struct queue_t* q,int remove_len)
{
	int len;
	
	len=get_queue_content_len(q);
	len=MIN(len,remove_len);
	
	q->h=(q->h+len)%q->buf_len;
	
	return len;
}

int pull_from_queue(struct queue_t* q,char* buf,int buf_len)
{
	int h=q->h;
	int i=0;
	
	while(i<buf_len) {
		if(h == q->t)
			break;
		
		buf[i++]=q->buf[h];
		h=(h+1)%q->buf_len;
	}
	
	return i;
}

int push_into_queue(struct queue_t* q,const char* data,int data_len)
{
	int t;
	int i=0;
	
	while(i<data_len) {
		q->buf[q->t]=data[i];
		
		t=(q->t+1)%q->buf_len;
		if(t == q->h)
			break;
		
		q->t=t;
		i++;
	}

	return i;
}


int init_queue(struct queue_t* q,int buf_len)
{
	memset(q,0,sizeof(*q));
	
	if( (q->buf=(char*)malloc(buf_len))<0 )
		return -1;
	
	q->buf_len=buf_len;
	
	return 0;
}
	


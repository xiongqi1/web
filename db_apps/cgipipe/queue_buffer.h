#ifndef __QUEUE_BUFFER_H__
#define __QUEUE_BUFFER_H__

struct queue_t {
	int h;
	int t;
	
	char* buf;
	int buf_len;
};

void fini_queue(struct queue_t* q);
int get_queue_content_len(struct queue_t* q);
int get_queue_free(struct queue_t* q);
int remove_from_queue(struct queue_t* q,int remove_len);
int pull_from_queue(struct queue_t* q,char* buf,int buf_len);
int push_into_queue(struct queue_t* q,const char* data,int data_len);
int init_queue(struct queue_t* q,int buf_len);

		
#endif


#include "minilib.h"

#include "binqueue.h"

int binqueue_get_free_len(struct binqueue_t* queue)
{
	int used;

	used=(queue->t+queue->buf_len-queue->h)%queue->buf_len;

	return queue->buf_len-used-1;
}

void binqueue_clear(struct binqueue_t* queue)
{
	queue->h = queue->t = 0;
}

int binqueue_is_empty(struct binqueue_t* queue)
{
	return queue->h == queue->t;
}

void binqueue_waste(struct binqueue_t* queue, int waste)
{
	queue->h = (queue->h + waste) % queue->buf_len;
}

int binqueue_peek(struct binqueue_t* queue, void* dst, int dst_len)
{
	char* ptr = (char*)dst;
	int h = queue->h;

	while (ptr<(char*)dst+dst_len)	{
		if (h == queue->t)
			break;

		// copy
		*ptr++ = queue->buf[h];

		// inc. hdr
		h = (h+1) % queue->buf_len;
	}

	return (int)(ptr -(char*)dst);
}

int binqueue_write(struct binqueue_t* queue, const void* src, int src_len)
{
	char* ptr = (char*)src;
	int iNewT;

	while (ptr<(char*)src+src_len) {
		// get new T
		iNewT = (queue->t + 1) % queue->buf_len;
		if (iNewT == queue->h)
			break;

		// copy
		queue->buf[queue->t] = *ptr++;

		// inc. tail
		queue->t = iNewT;
	}

	return (int)(ptr -(char*)src);
}

void binqueue_destroy(struct binqueue_t* queue)
{
	if(!queue)
		return;

	_free(queue->buf);
	_free(queue);
}

struct binqueue_t* binqueue_create(int len)
{
	struct binqueue_t* queue;

	// create the object
	queue=(struct binqueue_t*)_malloc(sizeof(struct binqueue_t));
	if(!queue) {
		SYSLOG(LOG_ERROR,"failed to allocate binqueue_t - size=%d",sizeof(struct binqueue_t));
		goto err;
	}

	len++;
	queue->buf=_malloc(len);
	if(!queue->buf) {
		SYSLOG(LOG_ERROR,"failed to allocate buf - size=%d",len);
		goto err;
	}

	queue->buf_len=len;

	return queue;

err:
	binqueue_destroy(queue);
	return NULL;
}


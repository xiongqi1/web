#ifndef __BINQUEUE_H__
#define __BINQUEUE_H__

struct binqueue_t {
	int h;
	int t;

	char* buf;
	int buf_len;
};

struct binqueue_t* binqueue_create(int len);
void binqueue_destroy(struct binqueue_t* queue);

void binqueue_clear(struct binqueue_t* queue);
int binqueue_is_empty(struct binqueue_t* queue);

int binqueue_peek(struct binqueue_t* queue, void* dst, int dst_len);
void binqueue_waste(struct binqueue_t* queue, int waste);
int binqueue_write(struct binqueue_t* queue, const void* src, int src_len);

int binqueue_get_free_len(struct binqueue_t* queue);

#endif

#include <stdlib.h>

#include "cbuf.h"

/* Creates a buffer */
struct cbuf *cbuf_alloc(size_t size)
{
	struct cbuf *cb;
	cb=malloc(sizeof(struct cbuf)+size+1);
	if (cb) {
		cb->bsize=size+1;
		cb->head = cb->tail = size-1; /* Catch wrapping bugs faster */
		cb->newlines = 0;
	}
	return cb;
}

/* Frees a buffer */
void cbuf_free(struct cbuf *cb)
{
	free(cb);
}


/* Add data to buffer. Returns error if there isn't enough space. */
int cbuf_push(struct cbuf *cb, const char *data, size_t bytes)
{
	if (bytes>cbuf_empty(cb)) return -1;
	while (bytes) {
		if (*data == '\n') cb->newlines++;
		cb->d[cb->head++] = *data++;
		if (cb->head >= cb->bsize) cb->head=0;
		bytes--;
	}
	return 0;
}

/* Pulls data from buffer, up to maxbytes. Returns number of bytes read. */
size_t cbuf_pull(struct cbuf *cb, char *data, size_t maxbytes)
{
	size_t bytes=0;
	while (maxbytes && cb->tail != cb->head) {
		if (cb->d[cb->tail] == '\n') cb->newlines--;
		*data++ = cb->d[cb->tail++];
		if (cb->tail >= cb->bsize) cb->tail=0;
		maxbytes--; bytes++;
	}
	return bytes;
}

/* Add 0-terminated string to buffer. Returns error if there isn't enough space. */
int cbuf_puts(struct cbuf *cb, const char *data)
{
	int old_newlines=cb->newlines;
	size_t old_head=cb->head;
	while (*data && cb->head != cb->tail) {
		if (*data == '\n') cb->newlines++;
		cb->d[cb->head++] = *data++;
		if (cb->head >= cb->bsize) cb->head=0;
	}
	if (*data) {
		cb->head = old_head;
		cb->newlines = old_newlines;
		return -1;
	}
	return 0;
}

/* Pulls line from buffer, up to next end of line or maxbytes.
   Does not return incomplete lines. Adds terminating '\0' insted of '\n'. Returns strlen. */
size_t cbuf_getline(struct cbuf *cb, char *data, size_t maxbytes)
{
	size_t bytes=0;
	if (cb->newlines <= 0 || !maxbytes) return 0;
	maxbytes--;
	while (maxbytes) {
		if (cb->d[cb->tail] == '\n') {
			cb->tail++;
			cb->newlines--;
			break;
		}
		*data++ = cb->d[cb->tail++];
		if (cb->tail >= cb->bsize) cb->tail=0;
		maxbytes--; bytes++;
	}
	*data = '\0';
	return bytes;
}


/* Returns number of free bytes */
size_t cbuf_empty(struct cbuf *cb)
{
	return cb->bsize-cbuf_full(cb)-1;
}

/* Returns number of bytes in buffer */
size_t cbuf_full(struct cbuf *cb)
{
	signed long size;
	size=cb->head - cb->tail;
	if (size < 0) size+=cb->bsize;
	return size;
}

#ifdef TEST

void dump(struct cbuf *cb)
{
	size_t i;
	int cr;
	printf("	cbuf=%p\n",(void*)cb);
	printf("	bsize=%d, head=%d, tail=%d, newlines=%d\n",cb->bsize,cb->head,cb->tail,cb->newlines);
	printf("	empty()=%d, full()=%d\n",cbuf_empty(cb),cbuf_full(cb));
	cr=0;
	printf("	");
	for (i=0; i<cb->bsize; i++) {
		if (cb->d[i]<' ') printf(".");
		else printf("%c",cb->d[i]);
		cr++;
		if (cr>=32) {
			cr=0;
			printf("\n	");
		}
	}
	if (cr) printf("\n");
}

int main(void)
{
	#define SIZE 8
	#define RCHECK(cmd) do { int rval = cmd; printf(#cmd "->%d\n",rval); } while(0)
	struct cbuf *cb;
	char testb[SIZE+1];

	cb = cbuf_alloc(SIZE);
	dump(cb);
	RCHECK(cbuf_push(cb,"Hel\nlo",6));
	dump(cb);
	RCHECK(cbuf_push(cb,"Hello",5));
	dump(cb);
	RCHECK(cbuf_pull(cb,testb,2));
	dump(cb);
	RCHECK(cbuf_pull(cb,testb,8));
	dump(cb);
	RCHECK(cbuf_push(cb,"Hll\n",4));
	dump(cb);
	RCHECK(cbuf_puts(cb,"Wrl\n"));
	dump(cb);
	RCHECK(cbuf_puts(cb,"Wrl\n"));
	dump(cb);
	RCHECK(cbuf_getline(cb,testb,8));
	printf(">%s<\n",testb);
	dump(cb);
	RCHECK(cbuf_getline(cb,testb,8));
	printf(">%s<\n",testb);
	dump(cb);
	RCHECK(cbuf_getline(cb,testb,8));
	dump(cb);
	cbuf_free(cb);
	return 0;
}
#endif

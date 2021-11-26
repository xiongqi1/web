
#include <stdio.h>

struct cbuf {
	size_t bsize;
	size_t head, tail;
	int  newlines;
	char d[1]; /* Gets extended during allocation */
};

/* Creates a buffer */
struct cbuf *cbuf_alloc(size_t size);

/* Frees a buffer */
void cbuf_free(struct cbuf *cb);


/* Add data to buffer. Returns error if there isn't enough space. */
int cbuf_push(struct cbuf *cb, const char *data, size_t bytes);

/* Pulls data from buffer, up to maxbytes. Returns number of bytes read. */
size_t cbuf_pull(struct cbuf *cb, char *data, size_t maxbytes);


/* Add 0-terminated string to buffer. Returns error if there isn't enough space. */
int cbuf_puts(struct cbuf *cb, const char *data);

/* Pulls line from buffer, up to next end of line or maxbytes.
   Does not return incomplete lines. Adds terminating '\0'. Returns strlen.
*/
size_t cbuf_getline(struct cbuf *cb, char *data, size_t maxbytes);


/* Returns number of free bytes */
size_t cbuf_empty(struct cbuf *cb);

/* Returns number of bytes in buffer */
size_t cbuf_full(struct cbuf *cb);


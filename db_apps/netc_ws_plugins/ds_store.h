#ifndef DS_STORE_DEFINED
#define DS_STORE_DEFINED

typedef struct ds_store {
	ssize_t end_index;
	ssize_t max_size;
	unsigned char *data;
} ds_store;

#define ds_init(p, initial)  memset((p), 0, sizeof *(p)),ds_prepare((p), initial)
#define ds_size_data(p)  ((p)->end_index)
#define ds_data(p)  ((p)->data)

/* clear data and optionally move data up if there is a residual */
void ds_clear(ds_store * dsptr, ssize_t save_amount);

void ds_free(ds_store * dsptr);

/* resize data exactly, can be less, can be 0. Returns -ve on alloc failure */
ssize_t ds_resize(ds_store *p, ssize_t need);

/* prepare the buffer for at least this amount. Be generous. There is a s use case for
 * getting a buffer and holding it.
 * Returns -1 for error, or the amount that can be added (can be larger than requested)
 */

ssize_t ds_prepare(ds_store *p, ssize_t extra);
ssize_t ds_move_ptr(ds_store *p, ssize_t extra);
ssize_t ds_memset(ds_store *p, ssize_t amount, int c);

/* copy data, preparing the buffer */
ssize_t ds_cat(ds_store * p, void *data, ssize_t amount);
/* copy data, preparing the buffer */
ssize_t ds_strcat(ds_store * p, char *str);
ssize_t ds_catstrs(ds_store *p, ...);

#endif
